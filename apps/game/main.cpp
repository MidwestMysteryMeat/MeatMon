// MeatMon demo app: grid-stepped overworld with NPC/Trainer entities loaded
// from the map JSON, a dialogue box, and in-window battles. Talk to the
// trainer (Z) to fight their authored team; B starts the same fight directly.
// --selftest auto-plays a battle and exits 0/1 for CI.

#include "battle_scene.hpp"
#include "editor_ui.hpp"

#include <meatmon/app.hpp>
#include <meatmon/assets.hpp>
#include <meatmon/font.hpp>
#include <meatmon/sprites.hpp>
#include <meatmon/tilemap.hpp>
#include <meatmon/battle/battle.hpp>
#include <meatmon/battle/rng.hpp>
#include <meatmon/battle/team.hpp>

#include <SDL3/SDL_main.h>
#include <stb_image_write.h>       // implementation lives in editor_ui.cpp

#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>

using namespace mm;

namespace {

struct Player {
    int tx = 3, ty = 3;             // tile currently occupied
    int ttx = 3, tty = 3;           // tile being stepped onto
    int fdx = 0, fdy = 1;           // facing direction
    float t = 1.f;                  // step progress, 1 = aligned
    float prevX = 0, prevY = 0;     // pixel pos last tick (for interpolation)
    float curX = 0, curY = 0;       // pixel pos this tick
};

class DemoGame : public IGame {
public:
    DemoGame(std::filesystem::path gameDir, bool selftest,
             std::map<uint64_t, std::string> shots = {})
        : gameDir_(std::move(gameDir)), selftest_(selftest),
          shotSchedule_(std::move(shots)) {}

    bool sawBattleMove() const { return sawBattleMove_; }

    bool init(App& app) override {
        app_ = &app;
        assets_ = std::make_unique<AssetManager>(app.renderer(), gameDir_ / "assets");
        spriteLib_ = std::make_unique<SpriteLibrary>(gameDir_ / "assets");

        mapPath_ = gameDir_ / "maps" / "demo.json";
        if (!Tilemap::load(mapPath_, map_)) {
            SDL_Log("failed to load map %s", mapPath_.string().c_str());
            return false;
        }
        std::error_code ec;
        mapMtime_ = std::filesystem::last_write_time(mapPath_, ec);

        tileset_ = assets_->texture(map_.tilesetPath);
        playerTex_ = assets_->texture(spriteLib_->resolve({.slug = "player"}).string());
        font_.tex = assets_->texture("fonts/mono.png");

        try {
            dex_ = std::make_unique<battle::Dex>(battle::Dex::load(gameDir_ / "data"));
        } catch (const std::exception& e) {
            SDL_Log("Dex load failed: %s", e.what());
            return false;
        }
        if (!loadPlayer()) return false;
        if (!selftest_ && loadGame()) {      // resume; selftest stays pristine
            std::puts("[MeatMon] save loaded (F5 saves, F9 reloads)");
        }

        const float ts = static_cast<float>(map_.tileSize);
        player_.curX = player_.prevX = player_.tx * ts;
        player_.curY = player_.prevY = player_.ty * ts;

        std::puts("[MeatMon] arrows/WASD move | Z = talk | B = quick battle | F1 = editor | Esc = quit");
        std::puts("[MeatMon] in battle: arrows pick a move, Z/Enter confirms, Esc flees");
        return tileset_ != nullptr && playerTex_ != nullptr;
    }

    void handleEvent(const SDL_Event& ev) override {
        if (ev.type == SDL_EVENT_QUIT && !selftest_) saveGame();   // autosave
        if (editorOpen_) {
            if (editor_) editor_->processEvent(ev);
            if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat &&
                ev.key.key == SDLK_F1) {
                setEditorOpen(false);
            }
            return;
        }
        if (mode_ == Mode::Battle && battleScene_) {
            battleScene_->handleEvent(ev);
            return;
        }
        if (ev.type != SDL_EVENT_KEY_DOWN || ev.key.repeat) return;
        SDL_Keycode k = ev.key.key;
        if (k == SDLK_F1 && mode_ == Mode::Overworld) {
            setEditorOpen(true);
            return;
        }
        bool confirm = k == SDLK_Z || k == SDLK_RETURN || k == SDLK_SPACE;

        if (mode_ == Mode::Dialogue) {
            if (confirm) advanceDialogue();
            return;
        }
        if (mode_ != Mode::Overworld) return;

        if (k == SDLK_ESCAPE) {
            if (!selftest_) saveGame();      // autosave on quit
            app_->quit();
        } else if (confirm) {
            tryInteract();
        } else if (k == SDLK_B) {
            const MapEntity* t = firstTrainer();
            if (t) beginTrainerBattle(*t);
        } else if (k == SDLK_F5) {
            saveGame();
            toast("(Game saved.)");
        } else if (k == SDLK_F9) {
            toast(loadGame() ? "(Game loaded.)" : "(No save found.)");
        }
    }

    void update(double dt) override {
        ++ticks_;
        assets_->pollHotReload(ticks_ / 60.0);
        if (!editorOpen_) pollMapReload();   // editor owns the map while open
        if (selftest_) {                     // editor smoke test in CI
            if (ticks_ == 420) setEditorOpen(true);
            else if (ticks_ == 540) setEditorOpen(false);
        }
        if (editorOpen_) return;

        if (selftest_ && ticks_ == 30 && mode_ == Mode::Overworld) {
            const MapEntity* t = firstTrainer();
            if (t) beginTrainerBattle(*t);
        }

        switch (mode_) {
        case Mode::Overworld:
            updateOverworld(dt);
            break;
        case Mode::Dialogue:
            break;                              // input-driven
        case Mode::ToBattle:
            if (++transitionT_ >= 24) startPendingBattle();
            break;
        case Mode::Battle:
            if (battleScene_) {
                battleScene_->update();
                if (battleScene_->sawMove()) sawBattleMove_ = true;
                if (battleScene_->finished()) endBattle();
            }
            break;
        }
    }

    void render(SDL_Renderer* r, float alpha) override {
        if (editorOpen_ && editor_) {
            editor_->render();
        } else if (mode_ == Mode::Battle && battleScene_) {
            battleScene_->render(r, font_);
        } else {
            SDL_SetRenderDrawColor(r, 16, 20, 32, 255);
            SDL_RenderClear(r);
            if (tileset_) map_.draw(r, *tileset_, 0.f, 0.f);

            for (const auto& e : map_.entities) {   // NPCs under the player layer
                Texture* tex = assets_->texture(
                    spriteLib_->resolve({.slug = e.sprite}).string());
                if (!tex) continue;
                SDL_FRect dst{static_cast<float>(e.x * map_.tileSize),
                              static_cast<float>(e.y * map_.tileSize), 16.f, 16.f};
                SDL_RenderTexture(r, tex->handle, nullptr, &dst);
            }

            if (playerTex_) {
                float x = player_.prevX + (player_.curX - player_.prevX) * alpha;
                float y = player_.prevY + (player_.curY - player_.prevY) * alpha;
                SDL_FRect dst{x, y, 16.f, 16.f};
                SDL_RenderTexture(r, playerTex_->handle, nullptr, &dst);
            }

            if (mode_ == Mode::Dialogue) {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 16, 18, 28, 245);
                SDL_FRect panel{4, 144, 312, 44};
                SDL_RenderFillRect(r, &panel);
                SDL_SetRenderDrawColor(r, 120, 132, 168, 255);
                SDL_RenderRect(r, &panel);
                if (!dlgSpeaker_.empty()) {
                    font_.draw(r, dlgSpeaker_, 10, 146, {255, 224, 96, 255});
                }
                if (!dlg_.empty()) font_.draw(r, wrapText(dlg_.front(), 37), 10, 159);
            }

            if (mode_ == Mode::ToBattle) {       // strobe flash into the battle
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                Uint8 a = (transitionT_ / 4) % 2 ? 235 : 60;
                SDL_SetRenderDrawColor(r, 250, 250, 255, a);
                SDL_FRect full{0, 0, 320, 192};
                SDL_RenderFillRect(r, &full);
            }
        }
        maybeCaptureShot();
    }

private:
    enum class Mode { Overworld, Dialogue, ToBattle, Battle };

    void setEditorOpen(bool open) {
        if (open && !editor_) {
            EditorUI::Hooks hooks;
            hooks.dex = [this]() -> const battle::Dex& { return *dex_; };
            hooks.reloadData = [this] {
                try {
                    dex_ = std::make_unique<battle::Dex>(
                        battle::Dex::load(gameDir_ / "data"));
                } catch (const std::exception& e) {
                    SDL_Log("Dex reload failed: %s", e.what());
                }
                loadPlayer();
            };
            editor_ = std::make_unique<EditorUI>(*app_, *assets_, *spriteLib_,
                                                 map_, gameDir_, mapPath_, hooks);
            if (!editor_->init()) {
                SDL_Log("editor init failed");
                editor_.reset();
                return;
            }
        }
        editorOpen_ = open;
        // The editor draws at native window resolution; gameplay uses the
        // integer-scaled 320x192 logical canvas.
        if (open) {
            SDL_SetRenderLogicalPresentation(app_->renderer(), 0, 0,
                                             SDL_LOGICAL_PRESENTATION_DISABLED);
        } else {
            SDL_SetRenderLogicalPresentation(app_->renderer(), 320, 192,
                                             SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        }
    }

    bool loadPlayer() {
        std::ifstream f(gameDir_ / "data" / "player.json");
        if (!f) {
            SDL_Log("missing %s", (gameDir_ / "data" / "player.json").string().c_str());
            return false;
        }
        auto j = nlohmann::json::parse(f, nullptr, false);
        if (j.is_discarded()) return false;
        playerName_ = j.value("name", "Player");
        playerTeam_ = battle::teamFromJson(j["team"]);
        return !playerTeam_.empty();
    }

    const MapEntity* entityAt(int tx, int ty) const {
        for (const auto& e : map_.entities) {
            if (e.x == tx && e.y == ty) return &e;
        }
        return nullptr;
    }

    const MapEntity* firstTrainer() const {
        for (const auto& e : map_.entities) {
            if (e.type == "trainer") return &e;
        }
        return nullptr;
    }

    void tryInteract() {
        const MapEntity* e = entityAt(player_.tx + player_.fdx,
                                      player_.ty + player_.fdy);
        if (!e) return;
        bool beaten = defeated_.count(e->id) > 0;
        const char* key = (e->type == "trainer" && beaten) ? "defeatedDialogue"
                                                           : "dialogue";
        dlg_.clear();
        if (e->extra.contains(key)) {
            for (const auto& line : e->extra[key]) {
                dlg_.push_back(line.get<std::string>());
            }
        }
        if (dlg_.empty()) dlg_.push_back("...");
        dlgSpeaker_ = e->extra.value("name", e->id);
        pendingTrainer_ = (e->type == "trainer" && !beaten) ? *e : MapEntity{};
        pendingHeal_ = e->extra.value("heals", false);
        mode_ = Mode::Dialogue;
    }

    void advanceDialogue() {
        if (!dlg_.empty()) dlg_.pop_front();
        if (!dlg_.empty()) return;
        if (pendingHeal_) {
            pendingHeal_ = false;
            healParty();
            dlg_.push_back("(Your team was fully healed!)");
            return;
        }
        if (pendingTrainer_.type == "trainer") {
            if (!partyAlive()) {
                pendingTrainer_ = MapEntity{};
                dlg_.push_back("(Your team is in no shape to battle...)");
                return;
            }
            mode_ = Mode::ToBattle;
            transitionT_ = 0;
        } else {
            mode_ = Mode::Overworld;
        }
    }

    void toast(std::string msg) {
        dlgSpeaker_.clear();
        dlg_.clear();
        dlg_.push_back(std::move(msg));
        pendingTrainer_ = MapEntity{};
        pendingHeal_ = false;
        mode_ = Mode::Dialogue;
    }

    bool partyAlive() const {
        for (const auto& s : playerTeam_) {
            if (s.hp != 0) return true;      // -1 = full, >0 = damaged
        }
        return false;
    }

    void healParty() {
        for (auto& s : playerTeam_) {
            s.hp = -1;
            s.status.clear();
        }
    }

    void beginTrainerBattle(const MapEntity& trainer) {
        pendingTrainer_ = trainer;
        pendingWild_ = false;
        mode_ = Mode::ToBattle;
        transitionT_ = 0;
    }

    void startPendingBattle() {
        battleScene_ = std::make_unique<BattleScene>(*assets_, *spriteLib_, *dex_,
                                                     selftest_);
        if (pendingWild_) {
            battleScene_->start(0x5EED0000ULL + ticks_, playerName_, playerTeam_,
                                "Wild", {wildMon_}, /*wild=*/true);
            mode_ = Mode::Battle;
            return;
        }
        auto foeTeam = battle::teamFromJson(pendingTrainer_.extra["team"]);
        if (foeTeam.empty()) {
            SDL_Log("trainer %s has no team", pendingTrainer_.id.c_str());
            mode_ = Mode::Overworld;
            return;
        }
        battleScene_->start(0x5EED0000ULL + ticks_, playerName_, playerTeam_,
                            pendingTrainer_.extra.value("name", "Trainer"), foeTeam);
        mode_ = Mode::Battle;
    }

    void endBattle() {
        std::string levelMsg;
        if (const auto* b = battleScene_->battle()) {   // party HP/status carry
            const auto& mons = b->side(0).monsters;
            for (size_t i = 0; i < playerTeam_.size() && i < mons.size(); ++i) {
                playerTeam_[i].hp = mons[i].hp;
                playerTeam_[i].status = mons[i].status;
            }
            if (battleScene_->playerWon()) {            // EXP for the mon that won
                int idx = b->side(0).active;
                int foeIdx = b->side(1).active;
                if (idx >= 0 && idx < static_cast<int>(playerTeam_.size()) && foeIdx >= 0) {
                    const auto& winner = b->side(0).monsters[idx];
                    const auto& foe = b->side(1).monsters[foeIdx];
                    int gained = battle::expYieldFor(*foe.species, foe.level);
                    int levels = battle::gainExp(playerTeam_[idx], gained);
                    if (levels > 0) {
                        levelMsg = winner.name + " grew to Lv. " +
                                   std::to_string(playerTeam_[idx].level) + "!";
                    }
                }
            }
        }
        if (battleScene_->playerWon() && !pendingTrainer_.id.empty()) {
            defeated_.insert(pendingTrainer_.id);
        }
        if (battleScene_->caught() && playerTeam_.size() < 6) {
            playerTeam_.push_back(battleScene_->caughtSet());
        }
        pendingTrainer_ = MapEntity{};
        pendingWild_ = false;
        battleScene_.reset();
        if (!partyAlive()) {
            healParty();
            dlgSpeaker_.clear();
            dlg_.clear();
            dlg_.push_back("You have no monsters left...");
            dlg_.push_back("(Your team was rushed to rest and fully healed.)");
            mode_ = Mode::Dialogue;
        } else if (!levelMsg.empty()) {
            toast(levelMsg);
        } else {
            mode_ = Mode::Overworld;
        }
    }

    void saveGame() {
        nlohmann::json j;
        j["save_version"] = 1;
        j["map"] = mapName_;
        j["player"] = {{"x", player_.tx}, {"y", player_.ty},
                       {"fdx", player_.fdx}, {"fdy", player_.fdy}};
        j["playtime_ticks"] = playtimeBase_ + ticks_;
        j["defeated"] = defeated_;
        j["party"] = battle::teamToJson(playerTeam_);
        std::error_code ec;
        std::filesystem::create_directories(gameDir_ / "saves", ec);
        std::ofstream f(gameDir_ / "saves" / "save.json");
        if (f) {
            f << j.dump(2) << "\n";
            SDL_Log("saved game");
        } else {
            SDL_Log("save FAILED");
        }
    }

    bool loadGame() {
        std::ifstream f(gameDir_ / "saves" / "save.json");
        if (!f) return false;
        auto j = nlohmann::json::parse(f, nullptr, false);
        if (j.is_discarded() || j.value("save_version", 0) != 1) return false;
        std::string savedMap = j.value("map", mapName_);
        if (savedMap != mapName_) {
            std::filesystem::path path = gameDir_ / "maps" / (savedMap + ".json");
            Tilemap fresh;
            if (Tilemap::load(path, fresh)) {
                map_ = std::move(fresh);
                mapPath_ = path;
                std::error_code mec;
                mapMtime_ = std::filesystem::last_write_time(mapPath_, mec);
                tileset_ = assets_->texture(map_.tilesetPath);
                mapName_ = savedMap;
            }
        }
        player_.tx = player_.ttx = j["player"].value("x", 3);
        player_.ty = player_.tty = j["player"].value("y", 3);
        player_.fdx = j["player"].value("fdx", 0);
        player_.fdy = j["player"].value("fdy", 1);
        player_.t = 1.f;
        const float ts = static_cast<float>(map_.tileSize);
        player_.curX = player_.prevX = player_.tx * ts;
        player_.curY = player_.prevY = player_.ty * ts;
        defeated_.clear();
        for (const auto& id : j.value("defeated", std::vector<std::string>{})) {
            defeated_.insert(id);
        }
        if (j.contains("party")) {
            auto party = battle::teamFromJson(j["party"]);
            if (!party.empty()) playerTeam_ = std::move(party);
        }
        playtimeBase_ = j.value("playtime_ticks", 0ULL);
        return true;
    }

    void updateOverworld(double dt) {
        auto& p = player_;
        p.prevX = p.curX;
        p.prevY = p.curY;
        const float ts = static_cast<float>(map_.tileSize);

        if (p.t < 1.f) {
            p.t = std::min(1.f, p.t + static_cast<float>(dt) * 5.f);  // 5 tiles/s
            if (p.t >= 1.f) {
                p.tx = p.ttx;
                p.ty = p.tty;
                onArriveTile();       // may load a new map or start a battle
            }
        } else {
            p.tx = p.ttx;
            p.ty = p.tty;
            const bool* keys = SDL_GetKeyboardState(nullptr);
            int dx = 0, dy = 0;
            if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) dx = -1;
            else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) dx = 1;
            else if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) dy = -1;
            else if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) dy = 1;
            if (dx || dy) {
                p.fdx = dx;
                p.fdy = dy;
                if (!map_.solid(p.tx + dx, p.ty + dy) &&
                    !entityAt(p.tx + dx, p.ty + dy)) {
                    p.ttx = p.tx + dx;
                    p.tty = p.ty + dy;
                    p.t = 0.f;
                }
            }
        }

        float u = std::min(p.t, 1.f);
        p.curX = ((1 - u) * p.tx + u * p.ttx) * ts;
        p.curY = ((1 - u) * p.ty + u * p.tty) * ts;
    }

    // Fires once, the frame the player's step lands on a new tile: map warps
    // take priority over wild encounters (can't roll for grass mid-warp).
    void onArriveTile() {
        for (const auto& w : map_.warpList) {
            if (w.x == player_.tx && w.y == player_.ty) {
                loadMap(w.map, w.tx, w.ty);
                return;
            }
        }
        tryWildEncounter();
    }

    void loadMap(const std::string& name, int tx, int ty) {
        std::filesystem::path path = gameDir_ / "maps" / (name + ".json");
        Tilemap fresh;
        if (!Tilemap::load(path, fresh)) {
            SDL_Log("failed to load map %s", path.string().c_str());
            return;
        }
        map_ = std::move(fresh);
        mapPath_ = path;
        std::error_code ec;
        mapMtime_ = std::filesystem::last_write_time(mapPath_, ec);
        tileset_ = assets_->texture(map_.tilesetPath);
        mapName_ = name;

        player_.tx = player_.ttx = tx;
        player_.ty = player_.tty = ty;
        player_.t = 1.f;
        const float ts = static_cast<float>(map_.tileSize);
        player_.curX = player_.prevX = tx * ts;
        player_.curY = player_.prevY = ty * ts;
    }

    int groundTileAt(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= map_.width || ty >= map_.height) return 0;
        return map_.ground[static_cast<size_t>(ty) * map_.width + tx];
    }

    // `encounters`: { "tiles": [id...], "rate": 0..256, "table": [
    //   { "species", "min", "max", "weight", "moves": [...] }, ... ] }
    void tryWildEncounter() {
        if (!map_.encounters.is_object() || map_.encounters.empty()) return;
        auto tiles = map_.encounters.value("tiles", std::vector<int>{});
        int tileId = groundTileAt(player_.tx, player_.ty);
        if (std::find(tiles.begin(), tiles.end(), tileId) == tiles.end()) return;

        int rate = map_.encounters.value("rate", 0);   // out of 256
        if (rate <= 0 || encounterRng_.next(256) >= static_cast<uint32_t>(rate)) return;

        auto table = map_.encounters.value("table", nlohmann::json::array());
        int totalWeight = 0;
        for (const auto& e : table) totalWeight += e.value("weight", 1);
        if (totalWeight <= 0) return;

        int roll = static_cast<int>(encounterRng_.next(static_cast<uint32_t>(totalWeight)));
        for (const auto& e : table) {
            int w = e.value("weight", 1);
            if (roll < w) {
                beginWildEncounter(e);
                return;
            }
            roll -= w;
        }
    }

    void beginWildEncounter(const nlohmann::json& entry) {
        int lo = entry.value("min", 5), hi = entry.value("max", lo);
        wildMon_ = battle::MonsterSet{};
        wildMon_.species = entry.value("species", "");
        wildMon_.level = encounterRng_.range(lo, hi + 1);
        wildMon_.moves = entry.value("moves", std::vector<std::string>{});
        if (wildMon_.species.empty() || wildMon_.moves.empty()) return;

        pendingWild_ = true;
        pendingTrainer_ = MapEntity{};
        mode_ = Mode::ToBattle;
        transitionT_ = 0;
    }

    // Screenshot capture (--shot TICK:PATH, repeatable): fires from render()
    // so the frame drawn for `ticks_` has actually been issued to the
    // renderer before we read it back.
    void maybeCaptureShot() {
        if (shotSchedule_.empty()) return;
        auto it = shotSchedule_.find(ticks_);
        if (it == shotSchedule_.end()) return;
        captureShot(it->second);
        shotSchedule_.erase(it);
    }

    void captureShot(const std::string& path) {
        SDL_Surface* surf = SDL_RenderReadPixels(app_->renderer(), nullptr);
        if (!surf) {
            SDL_Log("screenshot failed: %s", SDL_GetError());
            return;
        }
        SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surf);
        if (!conv) {
            SDL_Log("screenshot convert failed: %s", SDL_GetError());
            return;
        }
        std::filesystem::path p(path);
        std::error_code ec;
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
        stbi_write_png(path.c_str(), conv->w, conv->h, 4, conv->pixels, conv->pitch);
        SDL_DestroySurface(conv);
        SDL_Log("[MeatMon] screenshot saved: %s", path.c_str());
    }

    void pollMapReload() {
        if (ticks_ % 30 != 0) return;
        std::error_code ec;
        auto m = std::filesystem::last_write_time(mapPath_, ec);
        if (ec || m == mapMtime_) return;
        mapMtime_ = m;
        Tilemap fresh;
        if (Tilemap::load(mapPath_, fresh)) {
            map_ = std::move(fresh);
            tileset_ = assets_->texture(map_.tilesetPath);
            SDL_Log("hot-reloaded %s", mapPath_.string().c_str());
        }
    }

    std::filesystem::path gameDir_;
    bool selftest_ = false;
    App* app_ = nullptr;

    std::unique_ptr<AssetManager> assets_;
    std::unique_ptr<SpriteLibrary> spriteLib_;
    Tilemap map_;
    std::string mapName_ = "demo";
    std::filesystem::path mapPath_;
    std::filesystem::file_time_type mapMtime_;
    Texture* tileset_ = nullptr;
    Texture* playerTex_ = nullptr;
    Font font_;
    Player player_;

    std::string playerName_ = "Player";
    std::vector<battle::MonsterSet> playerTeam_;
    std::unique_ptr<battle::Dex> dex_;
    std::unique_ptr<BattleScene> battleScene_;
    std::unique_ptr<EditorUI> editor_;
    bool editorOpen_ = false;
    Mode mode_ = Mode::Overworld;
    int transitionT_ = 0;
    bool sawBattleMove_ = false;

    std::deque<std::string> dlg_;
    std::string dlgSpeaker_;
    MapEntity pendingTrainer_;
    bool pendingHeal_ = false;
    std::set<std::string> defeated_;            // persisted in save.json
    uint64_t playtimeBase_ = 0;

    bool pendingWild_ = false;
    battle::MonsterSet wildMon_;
    battle::Prng encounterRng_{0x6E1C0053ULL};

    uint64_t ticks_ = 0;
    std::map<uint64_t, std::string> shotSchedule_;   // tick -> output path
};

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path gameDir = "game";
    bool selftest = false;
    std::map<uint64_t, std::string> shots;    // --shot TICK:PATH, repeatable
    uint64_t maxShotTick = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--game" && i + 1 < argc) gameDir = argv[++i];
        else if (a == "--selftest") selftest = true;
        else if (a == "--shot" && i + 1 < argc) {
            std::string spec = argv[++i];
            auto colon = spec.find(':');
            if (colon != std::string::npos) {
                uint64_t tick = std::stoull(spec.substr(0, colon));
                shots[tick] = spec.substr(colon + 1);
                maxShotTick = std::max(maxShotTick, tick);
            }
        }
    }

    AppConfig cfg;
    cfg.title = "MeatMon Engine";
    cfg.logicalW = 320;   // 20 x 16px tiles
    cfg.logicalH = 192;   // 12 x 16px tiles
    cfg.windowScale = 3;

    App app;
    if (!app.init(cfg)) return 1;

    DemoGame game(gameDir, selftest, shots);
    int maxFrames = selftest ? 600 : (!shots.empty() ? static_cast<int>(maxShotTick) + 30 : -1);
    int rc = app.run(game, maxFrames);

    if (selftest) {
        bool ok = rc == 0 && game.sawBattleMove();
        std::puts(ok ? "[selftest] OK" : "[selftest] FAILED");
        return ok ? 0 : 1;
    }
    return rc;
}
