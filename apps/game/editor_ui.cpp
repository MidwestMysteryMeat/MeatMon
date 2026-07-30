#include "editor_ui.hpp"

#include <meatmon/battle/team.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <nlohmann/json.hpp>

#include <stb_image.h>          // implementation lives in engine/texture.cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <fstream>
#include <sstream>

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

constexpr int kScale = 2;                  // map view zoom
constexpr float kMapX = 16.f, kMapY = 40.f;

const char* kTypes[] = {"normal", "fire", "water", "electric", "grass", "ice",
                        "fighting", "poison", "ground", "flying", "psychic",
                        "bug", "rock", "ghost", "dragon", "dark", "steel",
                        "fairy"};

ImTextureID toImTex(SDL_Texture* t) {
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(t));
}

bool comboIds(const char* label, std::string& value,
              const std::vector<std::string>& options, bool allowEmpty) {
    bool changed = false;
    std::string shown = value.empty() ? "(none)" : value;
    if (ImGui::BeginCombo(label, shown.c_str())) {
        if (allowEmpty && ImGui::Selectable("(none)", value.empty())) {
            value.clear();
            changed = true;
        }
        for (const auto& opt : options) {
            if (ImGui::Selectable(opt.c_str(), value == opt)) {
                value = opt;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool writeTextFile(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    if (!f) return false;
    f << text;
    return true;
}

std::string readTextFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

EditorUI::EditorUI(mm::App& app, mm::AssetManager& assets, mm::SpriteLibrary& sprites,
                   mm::Tilemap& map, fs::path gameDir, fs::path mapPath, Hooks hooks)
    : app_(app), assets_(assets), sprites_(sprites), map_(map),
      gameDir_(std::move(gameDir)), mapPath_(std::move(mapPath)),
      hooks_(std::move(hooks)) {}

EditorUI::~EditorUI() {
    if (spr_.tex) SDL_DestroyTexture(spr_.tex);
    if (ready_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
}

bool EditorUI::init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   // no imgui.ini litter
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForSDLRenderer(app_.window(), app_.renderer()) ||
        !ImGui_ImplSDLRenderer3_Init(app_.renderer())) {
        return false;
    }
    ready_ = true;
    status_ = "F1 closes the editor. All edits write to game/ and hot-reload.";
    return true;
}

void EditorUI::processEvent(const SDL_Event& ev) {
    if (ready_) ImGui_ImplSDL3_ProcessEvent(&ev);
}

// ---------------------------------------------------------------------------

void EditorUI::render() {
    SDL_Renderer* r = app_.renderer();
    SDL_SetRenderDrawColor(r, 28, 30, 40, 255);
    SDL_RenderClear(r);

    drawMapView();

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    handleMapMouse();
    drawMenuBar();
    if (showTiles_) drawTilesPanel();
    if (showEntities_) drawEntitiesPanel();
    if (showSpecies_) drawSpeciesPanel();
    if (showTeam_) drawTeamPanel();
    if (showBattle_) drawBattlePanel();
    if (showSprites_) drawSpritePanel();
    if (showFiles_) drawFilesPanel();

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), r);
}

void EditorUI::drawMapView() {
    SDL_Renderer* r = app_.renderer();
    mm::Texture* tileset = assets_.texture(map_.tilesetPath);
    const float ts = static_cast<float>(map_.tileSize) * kScale;

    auto drawLayer = [&](const std::vector<int>& layer) {
        if (!tileset) return;
        for (int y = 0; y < map_.height; ++y) {
            for (int x = 0; x < map_.width; ++x) {
                int idx = layer[static_cast<size_t>(y) * map_.width + x];
                if (idx <= 0) continue;
                SDL_FRect src{static_cast<float>((idx - 1) * map_.tileSize), 0.f,
                              static_cast<float>(map_.tileSize),
                              static_cast<float>(map_.tileSize)};
                SDL_FRect dst{kMapX + x * ts, kMapY + y * ts, ts, ts};
                SDL_RenderTexture(r, tileset->handle, &src, &dst);
            }
        }
    };
    drawLayer(map_.ground);
    drawLayer(map_.objects);

    for (size_t i = 0; i < map_.entities.size(); ++i) {
        const auto& e = map_.entities[i];
        mm::Texture* tex =
            assets_.texture(sprites_.resolve({.slug = e.sprite}).string());
        SDL_FRect dst{kMapX + e.x * ts, kMapY + e.y * ts, ts, ts};
        if (tex) SDL_RenderTexture(r, tex->handle, nullptr, &dst);
        if (static_cast<int>(i) == selEntity_) {
            SDL_SetRenderDrawColor(r, 255, 224, 96, 255);
            SDL_RenderRect(r, &dst);
        }
    }

    if (layer_ == 2) {                     // collision overlay
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 224, 60, 60, 90);
        for (int y = 0; y < map_.height; ++y) {
            for (int x = 0; x < map_.width; ++x) {
                if (!map_.collision[static_cast<size_t>(y) * map_.width + x]) continue;
                SDL_FRect dst{kMapX + x * ts, kMapY + y * ts, ts, ts};
                SDL_RenderFillRect(r, &dst);
            }
        }
    }

    SDL_SetRenderDrawColor(r, 70, 76, 96, 255);
    SDL_FRect frame{kMapX - 1, kMapY - 1, map_.width * ts + 2, map_.height * ts + 2};
    SDL_RenderRect(r, &frame);
}

void EditorUI::handleMapMouse() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    const float ts = static_cast<float>(map_.tileSize) * kScale;
    int tx = static_cast<int>((io.MousePos.x - kMapX) / ts);
    int ty = static_cast<int>((io.MousePos.y - kMapY) / ts);
    if (io.MousePos.x < kMapX || io.MousePos.y < kMapY ||
        tx < 0 || ty < 0 || tx >= map_.width || ty >= map_.height) {
        return;
    }

    if (placeMode_ && selEntity_ >= 0 &&
        selEntity_ < static_cast<int>(map_.entities.size()) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        map_.entities[selEntity_].x = tx;
        map_.entities[selEntity_].y = ty;
        placeMode_ = false;
        mapDirty_ = true;
        return;
    }

    std::vector<int>* layer = layer_ == 0 ? &map_.ground
                              : layer_ == 1 ? &map_.objects
                                            : &map_.collision;
    size_t at = static_cast<size_t>(ty) * map_.width + tx;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        int val = layer_ == 2 ? (brush_ > 0 ? 1 : 0) : brush_;
        if ((*layer)[at] != val) { (*layer)[at] = val; mapDirty_ = true; }
    } else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        if ((*layer)[at] != 0) { (*layer)[at] = 0; mapDirty_ = true; }
    }
}

void EditorUI::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save map", nullptr, false, mapDirty_)) saveMap();
        if (ImGui::MenuItem("Reload data (Dex + player)")) {
            hooks_.reloadData();
            selSpecies_ = -1;
            teamLoaded_ = false;
            status_ = "data reloaded";
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Panels")) {
        ImGui::MenuItem("Tiles", nullptr, &showTiles_);
        ImGui::MenuItem("Entities", nullptr, &showEntities_);
        ImGui::MenuItem("Species", nullptr, &showSpecies_);
        ImGui::MenuItem("Team builder", nullptr, &showTeam_);
        ImGui::MenuItem("Battle tester", nullptr, &showBattle_);
        ImGui::MenuItem("Sprite studio", nullptr, &showSprites_);
        ImGui::MenuItem("Files", nullptr, &showFiles_);
        ImGui::EndMenu();
    }
    ImGui::TextDisabled("%s%s", mapDirty_ ? "[map*] " : "", status_.c_str());
    ImGui::EndMainMenuBar();
}

void EditorUI::saveMap() {
    if (map_.save(mapPath_)) {
        mapDirty_ = false;
        status_ = "saved " + mapPath_.filename().string();
    } else {
        status_ = "FAILED to save map";
    }
}

// --- Tiles ------------------------------------------------------------------

void EditorUI::drawTilesPanel() {
    ImGui::SetNextWindowPos(ImVec2(672, 34), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(276, 190), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Tiles", &showTiles_)) { ImGui::End(); return; }

    ImGui::RadioButton("Ground", &layer_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Objects", &layer_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Collision", &layer_, 2);

    if (layer_ == 2) {
        ImGui::TextWrapped("LMB = solid, RMB = clear. Red overlay shows solids.");
        brush_ = 1;
    } else {
        mm::Texture* tileset = assets_.texture(map_.tilesetPath);
        int tiles = tileset ? tileset->w / map_.tileSize : 0;
        if (ImGui::Selectable("0: erase", brush_ == 0, 0, ImVec2(70, 18))) brush_ = 0;
        for (int i = 1; i <= tiles; ++i) {
            ImGui::PushID(i);
            float u0 = static_cast<float>((i - 1) * map_.tileSize) / tileset->w;
            float u1 = static_cast<float>(i * map_.tileSize) / tileset->w;
            bool sel = brush_ == i;
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.8f, 0.3f, 0.6f));
            if (ImGui::ImageButton("tile", toImTex(tileset->handle), ImVec2(28, 28),
                                   ImVec2(u0, 0.f), ImVec2(u1, 1.f))) {
                brush_ = i;
            }
            if (sel) ImGui::PopStyleColor();
            ImGui::PopID();
            if (i % 6 != 0) ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::TextWrapped("LMB paints tile %d, RMB erases.", brush_);
    }
    if (ImGui::Button("Save map")) saveMap();
    ImGui::End();
}

// --- Entities ---------------------------------------------------------------

void EditorUI::loadEntityBuffers() {
    entBufsFor_ = selEntity_;
    entId_ = entSprite_ = entName_ = entDialogue_ = entTeamJson_ = "";
    entType_ = 0;
    if (selEntity_ < 0 || selEntity_ >= static_cast<int>(map_.entities.size())) return;
    const auto& e = map_.entities[selEntity_];
    entId_ = e.id;
    entSprite_ = e.sprite;
    entType_ = e.type == "trainer" ? 1 : 0;
    entName_ = e.extra.value("name", "");
    if (e.extra.contains("dialogue")) {
        for (const auto& l : e.extra["dialogue"]) {
            if (!entDialogue_.empty()) entDialogue_ += '\n';
            entDialogue_ += l.get<std::string>();
        }
    }
    if (e.extra.contains("team")) entTeamJson_ = e.extra["team"].dump(2);
}

void EditorUI::drawEntitiesPanel() {
    ImGui::SetNextWindowPos(ImVec2(672, 230), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(276, 330), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Entities", &showEntities_)) { ImGui::End(); return; }

    for (size_t i = 0; i < map_.entities.size(); ++i) {
        const auto& e = map_.entities[i];
        std::string label = e.id + " (" + e.type + ")";
        if (ImGui::Selectable(label.c_str(), static_cast<int>(i) == selEntity_)) {
            selEntity_ = static_cast<int>(i);
        }
    }
    if (ImGui::Button("+ NPC")) {
        mm::MapEntity e;
        e.type = "npc";
        e.id = "npc" + std::to_string(map_.entities.size());
        e.sprite = "villager";
        e.x = 2; e.y = 2;
        e.extra = json{{"dialogue", json::array({"..."})}};
        map_.entities.push_back(std::move(e));
        selEntity_ = static_cast<int>(map_.entities.size()) - 1;
        mapDirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Trainer")) {
        mm::MapEntity e;
        e.type = "trainer";
        e.id = "trainer" + std::to_string(map_.entities.size());
        e.sprite = "rival";
        e.x = 4; e.y = 2;
        e.extra = json{{"name", "Trainer"},
                       {"dialogue", json::array({"Let's battle!"})},
                       {"team", json::array({{{"species", "puddlit"},
                                              {"level", 50},
                                              {"moves", json::array({"tackle"})}}})}};
        map_.entities.push_back(std::move(e));
        selEntity_ = static_cast<int>(map_.entities.size()) - 1;
        mapDirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selEntity_ >= 0 &&
        selEntity_ < static_cast<int>(map_.entities.size())) {
        map_.entities.erase(map_.entities.begin() + selEntity_);
        selEntity_ = -1;
        mapDirty_ = true;
    }

    if (selEntity_ < 0 || selEntity_ >= static_cast<int>(map_.entities.size())) {
        ImGui::End();
        return;
    }
    if (entBufsFor_ != selEntity_) loadEntityBuffers();
    auto& e = map_.entities[selEntity_];

    ImGui::SeparatorText("Edit");
    ImGui::InputInt("x", &e.x);
    ImGui::InputInt("y", &e.y);
    e.x = std::clamp(e.x, 0, map_.width - 1);
    e.y = std::clamp(e.y, 0, map_.height - 1);
    ImGui::Checkbox("Place with click on map", &placeMode_);
    if (ImGui::InputText("id", &entId_)) { e.id = entId_; mapDirty_ = true; }
    if (ImGui::InputText("sprite", &entSprite_)) { e.sprite = entSprite_; mapDirty_ = true; }
    if (ImGui::Combo("type", &entType_, "npc\0trainer\0")) {
        e.type = entType_ == 1 ? "trainer" : "npc";
        mapDirty_ = true;
    }
    if (ImGui::InputText("name", &entName_)) {
        e.extra["name"] = entName_;
        mapDirty_ = true;
    }
    ImGui::Text("dialogue (one line per line):");
    if (ImGui::InputTextMultiline("##dlg", &entDialogue_, ImVec2(-1, 60))) {
        json arr = json::array();
        std::istringstream ss(entDialogue_);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) arr.push_back(line);
        }
        e.extra["dialogue"] = std::move(arr);
        mapDirty_ = true;
    }
    if (e.type == "trainer") {
        ImGui::Text("team (JSON):");
        ImGui::InputTextMultiline("##team", &entTeamJson_, ImVec2(-1, 90));
        if (ImGui::Button("Apply team JSON")) {
            json t = json::parse(entTeamJson_, nullptr, false);
            if (t.is_array()) {
                e.extra["team"] = std::move(t);
                mapDirty_ = true;
                status_ = "team applied";
            } else {
                status_ = "team JSON invalid (must be an array)";
            }
        }
    }
    ImGui::End();
}

// --- Species ----------------------------------------------------------------

void EditorUI::loadSpeciesBuffers() {
    const auto all = hooks_.dex().allSpecies();
    if (selSpecies_ < 0 || selSpecies_ >= static_cast<int>(all.size())) return;
    const auto* sp = all[selSpecies_];
    spName_ = sp->name;
    spNum_ = sp->num;
    spType1_ = sp->types.size() > 0 ? sp->types[0] : "normal";
    spType2_ = sp->types.size() > 1 ? sp->types[1] : "";
    spStats_[0] = sp->baseStats.hp;
    spStats_[1] = sp->baseStats.atk;
    spStats_[2] = sp->baseStats.def;
    spStats_[3] = sp->baseStats.spa;
    spStats_[4] = sp->baseStats.spd;
    spStats_[5] = sp->baseStats.spe;
}

void EditorUI::drawSpeciesPanel() {
    ImGui::SetNextWindowPos(ImVec2(20, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Species", &showSpecies_)) { ImGui::End(); return; }

    const auto all = hooks_.dex().allSpecies();
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        std::string label = "#" + std::to_string(all[i]->num) + " " + all[i]->name;
        if (ImGui::Selectable(label.c_str(), selSpecies_ == i)) {
            selSpecies_ = i;
            loadSpeciesBuffers();
        }
    }
    ImGui::InputText("new id", &spNewId_);
    ImGui::SameLine();
    if (ImGui::Button("New") && !spNewId_.empty()) {
        json j = json::parse(readTextFile(gameDir_ / "data" / "species.json"),
                             nullptr, false);
        if (j.is_object() && !j.contains(spNewId_)) {
            j[spNewId_] = {{"name", spNewId_},
                           {"num", static_cast<int>(j.size()) + 1},
                           {"types", json::array({"normal"})},
                           {"baseStats", {{"hp", 50}, {"atk", 50}, {"def", 50},
                                          {"spa", 50}, {"spd", 50}, {"spe", 50}}}};
            writeTextFile(gameDir_ / "data" / "species.json", j.dump(2) + "\n");
            hooks_.reloadData();
            status_ = "species " + spNewId_ + " created";
        }
    }

    if (selSpecies_ < 0 || selSpecies_ >= static_cast<int>(all.size())) {
        ImGui::End();
        return;
    }
    const std::string id = all[selSpecies_]->id;
    ImGui::SeparatorText(id.c_str());
    ImGui::InputText("name", &spName_);
    ImGui::InputInt("num (sprite key)", &spNum_);
    std::vector<std::string> types(std::begin(kTypes), std::end(kTypes));
    comboIds("type 1", spType1_, types, false);
    comboIds("type 2", spType2_, types, true);
    const char* statNames[] = {"HP", "Atk", "Def", "SpA", "SpD", "Spe"};
    for (int i = 0; i < 6; ++i) {
        ImGui::SliderInt(statNames[i], &spStats_[i], 1, 255);
    }
    if (ImGui::Button("Apply & save species.json")) {
        json j = json::parse(readTextFile(gameDir_ / "data" / "species.json"),
                             nullptr, false);
        if (j.is_object()) {
            json types2 = json::array({spType1_});
            if (!spType2_.empty()) types2.push_back(spType2_);
            j[id] = {{"name", spName_}, {"num", spNum_}, {"types", types2},
                     {"baseStats", {{"hp", spStats_[0]}, {"atk", spStats_[1]},
                                    {"def", spStats_[2]}, {"spa", spStats_[3]},
                                    {"spd", spStats_[4]}, {"spe", spStats_[5]}}}};
            writeTextFile(gameDir_ / "data" / "species.json", j.dump(2) + "\n");
            hooks_.reloadData();
            status_ = "species.json saved + Dex reloaded";
        }
    }
    ImGui::TextWrapped("Sprites: drop art at assets/custom/%s/front.png "
                       "(or use Sprite studio).", id.c_str());
    ImGui::End();
}

// --- Team builder -----------------------------------------------------------

void EditorUI::ensureTeamLoaded() {
    if (teamLoaded_) return;
    teamLoaded_ = true;
    team_.clear();
    json j = json::parse(readTextFile(gameDir_ / "data" / "player.json"),
                         nullptr, false);
    if (!j.is_object()) return;
    teamPlayerName_ = j.value("name", "Player");
    for (const auto& m : j.value("team", json::array())) {
        TeamSlot s;
        s.species = m.value("species", "");
        s.level = m.value("level", 50);
        s.nature = m.value("nature", "hardy");
        s.ability = m.value("ability", "");
        s.item = m.value("item", "");
        auto mv = m.value("moves", std::vector<std::string>{});
        for (size_t k = 0; k < 4 && k < mv.size(); ++k) s.moves[k] = mv[k];
        team_.push_back(std::move(s));
    }
}

void EditorUI::drawTeamPanel() {
    ImGui::SetNextWindowPos(ImVec2(360, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Team builder", &showTeam_)) { ImGui::End(); return; }
    ensureTeamLoaded();
    const auto& dex = hooks_.dex();

    std::vector<std::string> speciesIds, moveIds, abilityIds, itemIds, natureIds;
    for (auto* s : dex.allSpecies()) speciesIds.push_back(s->id);
    for (auto* m : dex.allMoves()) moveIds.push_back(m->id);
    for (auto* a : dex.allAbilities()) abilityIds.push_back(a->id);
    for (auto* i : dex.allItems()) itemIds.push_back(i->id);
    for (auto* n : dex.allNatures()) natureIds.push_back(n->id);

    ImGui::InputText("player name", &teamPlayerName_);
    for (int i = 0; i < static_cast<int>(team_.size()); ++i) {
        ImGui::PushID(i);
        auto& s = team_[i];
        std::string hdr = "Slot " + std::to_string(i + 1) + ": " +
                          (s.species.empty() ? "(empty)" : s.species);
        if (ImGui::CollapsingHeader(hdr.c_str())) {
            comboIds("species", s.species, speciesIds, false);
            ImGui::SliderInt("level", &s.level, 1, 100);
            comboIds("nature", s.nature, natureIds, false);
            comboIds("ability", s.ability, abilityIds, true);
            comboIds("item", s.item, itemIds, true);
            for (int k = 0; k < 4; ++k) {
                ImGui::PushID(100 + k);
                comboIds(("move " + std::to_string(k + 1)).c_str(), s.moves[k],
                         moveIds, true);
                ImGui::PopID();
            }
            if (ImGui::Button("Remove slot")) {
                team_.erase(team_.begin() + i);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }
    if (team_.size() < 6 && ImGui::Button("+ Add member")) {
        TeamSlot s;
        s.species = speciesIds.empty() ? "" : speciesIds.front();
        s.moves[0] = "tackle";
        team_.push_back(std::move(s));
    }
    if (ImGui::Button("Save player.json")) {
        json arr = json::array();
        for (const auto& s : team_) {
            if (s.species.empty()) continue;
            json m = {{"species", s.species}, {"level", s.level},
                      {"nature", s.nature}};
            if (!s.ability.empty()) m["ability"] = s.ability;
            if (!s.item.empty()) m["item"] = s.item;
            json mv = json::array();
            for (const auto& mid : s.moves) {
                if (!mid.empty()) mv.push_back(mid);
            }
            m["moves"] = std::move(mv);
            arr.push_back(std::move(m));
        }
        json j = {{"name", teamPlayerName_}, {"team", std::move(arr)}};
        writeTextFile(gameDir_ / "data" / "player.json", j.dump(2) + "\n");
        hooks_.reloadData();
        status_ = "player.json saved";
    }
    ImGui::End();
}

// --- Battle tester ------------------------------------------------------------

void EditorUI::drawBattlePanel() {
    ImGui::SetNextWindowPos(ImVec2(120, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Battle tester", &showBattle_)) { ImGui::End(); return; }
    ensureTeamLoaded();

    std::vector<const mm::MapEntity*> trainers;
    for (const auto& e : map_.entities) {
        if (e.type == "trainer") trainers.push_back(&e);
    }
    ImGui::InputInt("seed", &seed_);
    if (trainers.empty()) {
        ImGui::TextWrapped("No trainer entities on this map.");
    } else {
        foeSel_ = std::clamp(foeSel_, 0, static_cast<int>(trainers.size()) - 1);
        if (ImGui::BeginCombo("opponent", trainers[foeSel_]->id.c_str())) {
            for (int i = 0; i < static_cast<int>(trainers.size()); ++i) {
                if (ImGui::Selectable(trainers[i]->id.c_str(), foeSel_ == i)) foeSel_ = i;
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Run headless battle")) {
            using namespace mm::battle;
            try {
                std::vector<MonsterSet> p1;
                for (const auto& s : team_) {
                    if (s.species.empty()) continue;
                    MonsterSet set;
                    set.species = s.species;
                    set.level = s.level;
                    set.nature = s.nature;
                    set.ability = s.ability;
                    set.item = s.item;
                    for (const auto& m : s.moves) {
                        if (!m.empty()) set.moves.push_back(m);
                    }
                    p1.push_back(std::move(set));
                }
                auto p2 = teamFromJson(trainers[foeSel_]->extra["team"]);
                Battle b(hooks_.dex(), Format{}, static_cast<uint64_t>(seed_));
                b.setPlayer(0, teamPlayerName_, p1);
                b.setPlayer(1, trainers[foeSel_]->extra.value("name", "Trainer"), p2);
                b.start();
                int guard = 0;
                while (!b.ended() && guard++ < 200) {
                    for (int s = 0; s < 2; ++s) {
                        Request req = b.request(s);
                        if (req.kind == Request::Kind::Move) {
                            int idx = 0;
                            for (int k = 0; k < static_cast<int>(req.moves.size()); ++k) {
                                if (req.moves[k].pp > 0) { idx = k; break; }
                            }
                            b.choose(s, {ChoiceKind::Move, idx});
                        } else if (req.kind == Request::Kind::Switch &&
                                   !req.switches.empty()) {
                            b.choose(s, {ChoiceKind::Switch, req.switches.front()});
                        }
                    }
                    b.commitTurn();
                }
                simLog_.clear();
                for (const auto& l : b.log()) { simLog_ += l; simLog_ += '\n'; }
            } catch (const std::exception& ex) {
                simLog_ = std::string("battle error: ") + ex.what();
            }
        }
    }
    ImGui::BeginChild("simlog", ImVec2(0, 0));
    ImGui::TextUnformatted(simLog_.c_str());
    ImGui::EndChild();
    ImGui::End();
}

// --- Sprite studio ------------------------------------------------------------

void EditorUI::sprUpdateTexture() {
    if (!spr_.tex && spr_.w > 0) {
        spr_.tex = SDL_CreateTexture(app_.renderer(), SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STATIC, spr_.w, spr_.h);
        SDL_SetTextureScaleMode(spr_.tex, SDL_SCALEMODE_NEAREST);
    }
    if (spr_.tex) SDL_UpdateTexture(spr_.tex, nullptr, spr_.px.data(), spr_.w * 4);
}

void EditorUI::sprLoad(const std::string& slug, bool back) {
    fs::path p = gameDir_ / "assets" / "custom" / slug /
                 (back ? "back.png" : "front.png");
    int w = 0, h = 0, comp = 0;
    stbi_uc* data = stbi_load(p.string().c_str(), &w, &h, &comp, 4);
    if (!data) { status_ = "cannot open " + p.string(); return; }
    if (spr_.tex) { SDL_DestroyTexture(spr_.tex); spr_.tex = nullptr; }
    spr_.slug = slug;
    spr_.back = back;
    spr_.w = w;
    spr_.h = h;
    spr_.px.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    spr_.open = true;
    sprUpdateTexture();
}

void EditorUI::sprNew(const std::string& slug, int size) {
    if (spr_.tex) { SDL_DestroyTexture(spr_.tex); spr_.tex = nullptr; }
    spr_.slug = slug;
    spr_.back = false;
    spr_.w = spr_.h = size;
    spr_.px.assign(static_cast<size_t>(size) * size * 4, 0);
    spr_.open = true;
    sprUpdateTexture();
}

void EditorUI::sprSave() {
    fs::path dir = gameDir_ / "assets" / "custom" / spr_.slug;
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path p = dir / (spr_.back ? "back.png" : "front.png");
    if (stbi_write_png(p.string().c_str(), spr_.w, spr_.h, 4, spr_.px.data(),
                       spr_.w * 4)) {
        status_ = "saved " + p.string() + " (hot-reloads in game)";
    } else {
        status_ = "FAILED to save " + p.string();
    }
}

void EditorUI::drawSpritePanel() {
    ImGui::SetNextWindowPos(ImVec2(180, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sprite studio", &showSprites_)) { ImGui::End(); return; }

    // existing custom sprites
    if (ImGui::BeginCombo("open", spr_.open ? spr_.slug.c_str() : "(pick)")) {
        std::error_code ec;
        for (const auto& d :
             fs::directory_iterator(gameDir_ / "assets" / "custom", ec)) {
            if (!d.is_directory()) continue;
            std::string slug = d.path().filename().string();
            if (ImGui::Selectable((slug + "/front").c_str())) sprLoad(slug, false);
            if (fs::exists(d.path() / "back.png") &&
                ImGui::Selectable((slug + "/back").c_str())) sprLoad(slug, true);
        }
        ImGui::EndCombo();
    }
    ImGui::InputText("new slug", &sprNewSlug_);
    ImGui::SameLine();
    if (ImGui::Button(sprNewSize_ == 48 ? "48px" : "16px")) {
        sprNewSize_ = sprNewSize_ == 48 ? 16 : 48;
    }
    ImGui::SameLine();
    if (ImGui::Button("New") && !sprNewSlug_.empty()) sprNew(sprNewSlug_, sprNewSize_);

    if (!spr_.open) { ImGui::End(); return; }

    ImGui::ColorEdit4("color", spr_.color,
                      ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::Text("%s/%s  %dx%d  LMB paint, RMB erase", spr_.slug.c_str(),
                spr_.back ? "back" : "front", spr_.w, spr_.h);

    float zoom = spr_.w <= 16 ? 16.f : 7.f;
    ImVec2 canvasSize(spr_.w * zoom, spr_.h * zoom);
    ImGui::Image(toImTex(spr_.tex), canvasSize);
    ImVec2 mn = ImGui::GetItemRectMin();
    if (ImGui::IsItemHovered()) {
        ImVec2 mp = ImGui::GetMousePos();
        int px = static_cast<int>((mp.x - mn.x) / zoom);
        int py = static_cast<int>((mp.y - mn.y) / zoom);
        if (px >= 0 && py >= 0 && px < spr_.w && py < spr_.h) {
            size_t at = (static_cast<size_t>(py) * spr_.w + px) * 4;
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                spr_.px[at + 0] = static_cast<unsigned char>(spr_.color[0] * 255);
                spr_.px[at + 1] = static_cast<unsigned char>(spr_.color[1] * 255);
                spr_.px[at + 2] = static_cast<unsigned char>(spr_.color[2] * 255);
                spr_.px[at + 3] = static_cast<unsigned char>(spr_.color[3] * 255);
                sprUpdateTexture();
            } else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                spr_.px[at + 0] = spr_.px[at + 1] = spr_.px[at + 2] = spr_.px[at + 3] = 0;
                sprUpdateTexture();
            }
        }
    }
    if (ImGui::Button("Save PNG")) sprSave();
    ImGui::SameLine();
    ImGui::TextDisabled("saves to assets/custom/%s/", spr_.slug.c_str());
    ImGui::End();
}

// --- Files ---------------------------------------------------------------------

void EditorUI::refreshFileList() {
    files_.clear();
    std::error_code ec;
    for (const char* sub : {"data", "scripts", "maps"}) {
        fs::path root = gameDir_ / sub;
        for (auto it = fs::recursive_directory_iterator(root, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file()) continue;
            auto ext = it->path().extension();
            if (ext != ".json" && ext != ".lua" && ext != ".md") continue;
            files_.push_back(
                fs::relative(it->path(), gameDir_, ec).generic_string());
        }
    }
    std::sort(files_.begin(), files_.end());
}

void EditorUI::drawFilesPanel() {
    ImGui::SetNextWindowPos(ImVec2(220, 80), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Files", &showFiles_)) { ImGui::End(); return; }
    if (files_.empty()) refreshFileList();
    if (ImGui::Button("Refresh list")) refreshFileList();

    if (ImGui::BeginCombo("file", curFile_.empty() ? "(pick)" : curFile_.c_str())) {
        for (const auto& f : files_) {
            if (ImGui::Selectable(f.c_str(), f == curFile_)) {
                curFile_ = f;
                fileBuf_ = readTextFile(gameDir_ / f);
                fileStatus_.clear();
            }
        }
        ImGui::EndCombo();
    }
    if (!curFile_.empty()) {
        ImGui::InputTextMultiline("##filebuf", &fileBuf_, ImVec2(-1, 320));
        if (ImGui::Button("Save")) {
            if (writeTextFile(gameDir_ / curFile_, fileBuf_)) {
                fileStatus_ = "saved (data files hot-reload via File > Reload data)";
            } else {
                fileStatus_ = "save FAILED";
            }
        }
        if (fs::path(curFile_).extension() == ".json") {
            ImGui::SameLine();
            if (ImGui::Button("Validate JSON")) {
                fileStatus_ = json::parse(fileBuf_, nullptr, false).is_discarded()
                                  ? "INVALID JSON"
                                  : "JSON OK";
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", fileStatus_.c_str());
    }
    ImGui::End();
}
