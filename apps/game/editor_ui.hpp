#pragma once

// In-engine editor (F1). Dear ImGui panels over a scaled map view:
//   Tiles    — paint ground/objects/collision layers, save the map
//   Entities — add/edit/place NPCs and trainers (dialogue, teams)
//   Species  — structured editor writing species.json + live Dex reload
//   Team     — player team builder writing player.json
//   Battle   — headless battle tester (any seed, any map trainer)
//   Sprites  — pixel editor saving PNGs straight into the hot-reload pipeline
//   Files    — raw text editor for game/ JSON, Lua, and Markdown
// Everything writes to the same game/ files the runtime loads; edits are live.

#include <meatmon/app.hpp>
#include <meatmon/assets.hpp>
#include <meatmon/sprites.hpp>
#include <meatmon/tilemap.hpp>
#include <meatmon/battle/dex.hpp>

#include <SDL3/SDL.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class EditorUI {
public:
    struct Hooks {
        std::function<const mm::battle::Dex&()> dex;   // current Dex (may reload)
        std::function<void()> reloadData;              // re-load Dex + player team
    };

    EditorUI(mm::App& app, mm::AssetManager& assets, mm::SpriteLibrary& sprites,
             mm::Tilemap& map, std::filesystem::path gameDir,
             std::filesystem::path mapPath, Hooks hooks);
    ~EditorUI();

    bool init();                        // create ImGui context + backends
    void processEvent(const SDL_Event& ev);
    void render();                      // draws the whole editor frame

private:
    // map view
    void drawMapView();
    void handleMapMouse();
    void drawMenuBar();
    // panels
    void drawTilesPanel();
    void drawEntitiesPanel();
    void drawSpeciesPanel();
    void drawTeamPanel();
    void drawBattlePanel();
    void drawSpritePanel();
    void drawFilesPanel();
    // helpers
    void saveMap();
    void loadEntityBuffers();
    void loadSpeciesBuffers();
    void ensureTeamLoaded();
    void refreshFileList();
    void sprLoad(const std::string& slug, bool back);
    void sprNew(const std::string& slug, int size);
    void sprSave();
    void sprUpdateTexture();

    mm::App& app_;
    mm::AssetManager& assets_;
    mm::SpriteLibrary& sprites_;
    mm::Tilemap& map_;
    std::filesystem::path gameDir_;
    std::filesystem::path mapPath_;
    Hooks hooks_;
    bool ready_ = false;
    std::string status_;                // menu-bar status message

    // panel visibility
    bool showTiles_ = true, showEntities_ = true, showSpecies_ = false,
         showTeam_ = false, showBattle_ = false, showSprites_ = false,
         showFiles_ = false;

    // tiles
    int layer_ = 0;                     // 0 ground, 1 objects, 2 collision
    int brush_ = 1;
    bool mapDirty_ = false;

    // entities
    int selEntity_ = -1;
    int entBufsFor_ = -2;
    bool placeMode_ = false;
    std::string entId_, entSprite_, entName_, entDialogue_, entTeamJson_;
    int entType_ = 0;                   // 0 npc, 1 trainer

    // species
    int selSpecies_ = -1;
    std::string spName_, spType1_, spType2_, spNewId_;
    int spNum_ = 0, spStats_[6] = {50, 50, 50, 50, 50, 50};

    // team builder
    bool teamLoaded_ = false;
    std::string teamPlayerName_ = "Player";
    struct TeamSlot {
        std::string species, nature = "hardy", ability, item;
        std::string moves[4];
        int level = 50;
    };
    std::vector<TeamSlot> team_;

    // battle tester
    int seed_ = 777;
    int foeSel_ = 0;
    std::string simLog_;

    // sprite editor
    struct SpriteEdit {
        bool open = false;
        std::string slug = "newmon";
        bool back = false;
        int w = 0, h = 0;
        std::vector<unsigned char> px;  // RGBA
        SDL_Texture* tex = nullptr;
        float color[4] = {0.9f, 0.3f, 0.2f, 1.f};
    } spr_;
    std::string sprNewSlug_ = "newmon";
    int sprNewSize_ = 48;

    // files
    std::vector<std::string> files_;    // relative to gameDir_
    std::string curFile_, fileBuf_, fileStatus_;
};
