// MeatMon demo app: grid-stepped overworld with interpolated rendering and
// hot-reload, plus a full in-window battle scene (B key) driven by the battle
// engine's request/choice protocol. --selftest auto-plays a battle and exits
// 0/1 for CI.

#include "battle_scene.hpp"

#include <meatmon/app.hpp>
#include <meatmon/assets.hpp>
#include <meatmon/font.hpp>
#include <meatmon/sprites.hpp>
#include <meatmon/tilemap.hpp>
#include <meatmon/battle/battle.hpp>

#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

using namespace mm;

namespace {

struct Player {
    int tx = 3, ty = 3;             // tile currently occupied
    int ttx = 3, tty = 3;           // tile being stepped onto
    float t = 1.f;                  // step progress, 1 = aligned
    float prevX = 0, prevY = 0;     // pixel pos last tick (for interpolation)
    float curX = 0, curY = 0;       // pixel pos this tick
};

class DemoGame : public IGame {
public:
    DemoGame(std::filesystem::path gameDir, bool selftest)
        : gameDir_(std::move(gameDir)), selftest_(selftest) {}

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

        const float ts = static_cast<float>(map_.tileSize);
        player_.curX = player_.prevX = player_.tx * ts;
        player_.curY = player_.prevY = player_.ty * ts;

        std::puts("[MeatMon] arrows/WASD move | B = battle | Esc = quit");
        std::puts("[MeatMon] in battle: arrows pick a move, Z/Enter confirms, Esc flees");
        return tileset_ != nullptr && playerTex_ != nullptr;
    }

    void handleEvent(const SDL_Event& ev) override {
        if (mode_ == Mode::Battle && battleScene_) {
            battleScene_->handleEvent(ev);
            return;
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
            if (ev.key.key == SDLK_ESCAPE) app_->quit();
            if (ev.key.key == SDLK_B && mode_ == Mode::Overworld) {
                mode_ = Mode::ToBattle;
                transitionT_ = 0;
            }
        }
    }

    void update(double dt) override {
        ++ticks_;
        assets_->pollHotReload(ticks_ / 60.0);
        pollMapReload();

        if (selftest_ && ticks_ == 30 && mode_ == Mode::Overworld) {
            mode_ = Mode::ToBattle;
            transitionT_ = 0;
        }

        switch (mode_) {
        case Mode::Overworld:
            updateOverworld(dt);
            break;
        case Mode::ToBattle:
            if (++transitionT_ >= 24) {
                battleScene_ = std::make_unique<BattleScene>(*assets_, *spriteLib_,
                                                             *dex_, selftest_);
                battleScene_->start(0x5EED0000ULL + ticks_);
                mode_ = Mode::Battle;
            }
            break;
        case Mode::Battle:
            if (battleScene_) {
                battleScene_->update();
                if (battleScene_->sawMove()) sawBattleMove_ = true;
                if (battleScene_->finished()) {
                    battleScene_.reset();
                    mode_ = Mode::Overworld;
                }
            }
            break;
        }
    }

    void render(SDL_Renderer* r, float alpha) override {
        if (mode_ == Mode::Battle && battleScene_) {
            battleScene_->render(r, font_);
            return;
        }

        SDL_SetRenderDrawColor(r, 16, 20, 32, 255);
        SDL_RenderClear(r);
        if (tileset_) map_.draw(r, *tileset_, 0.f, 0.f);
        if (playerTex_) {
            float x = player_.prevX + (player_.curX - player_.prevX) * alpha;
            float y = player_.prevY + (player_.curY - player_.prevY) * alpha;
            SDL_FRect dst{x, y, 16.f, 16.f};
            SDL_RenderTexture(r, playerTex_->handle, nullptr, &dst);
        }

        if (mode_ == Mode::ToBattle) {   // strobe flash into the battle
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            Uint8 a = (transitionT_ / 4) % 2 ? 235 : 60;
            SDL_SetRenderDrawColor(r, 250, 250, 255, a);
            SDL_FRect full{0, 0, 320, 192};
            SDL_RenderFillRect(r, &full);
        }
    }

private:
    enum class Mode { Overworld, ToBattle, Battle };

    void updateOverworld(double dt) {
        auto& p = player_;
        p.prevX = p.curX;
        p.prevY = p.curY;
        const float ts = static_cast<float>(map_.tileSize);

        if (p.t >= 1.f) {
            p.tx = p.ttx;
            p.ty = p.tty;
            const bool* keys = SDL_GetKeyboardState(nullptr);
            int dx = 0, dy = 0;
            if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) dx = -1;
            else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) dx = 1;
            else if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) dy = -1;
            else if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) dy = 1;
            if ((dx || dy) && !map_.solid(p.tx + dx, p.ty + dy)) {
                p.ttx = p.tx + dx;
                p.tty = p.ty + dy;
                p.t = 0.f;
            }
        } else {
            p.t = std::min(1.f, p.t + static_cast<float>(dt) * 5.f);  // 5 tiles/s
        }

        float u = std::min(p.t, 1.f);
        p.curX = ((1 - u) * p.tx + u * p.ttx) * ts;
        p.curY = ((1 - u) * p.ty + u * p.tty) * ts;
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
    std::filesystem::path mapPath_;
    std::filesystem::file_time_type mapMtime_;
    Texture* tileset_ = nullptr;
    Texture* playerTex_ = nullptr;
    Font font_;
    Player player_;

    std::unique_ptr<battle::Dex> dex_;
    std::unique_ptr<BattleScene> battleScene_;
    Mode mode_ = Mode::Overworld;
    int transitionT_ = 0;
    bool sawBattleMove_ = false;

    uint64_t ticks_ = 0;
};

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path gameDir = "game";
    bool selftest = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--game" && i + 1 < argc) gameDir = argv[++i];
        else if (a == "--selftest") selftest = true;
    }

    AppConfig cfg;
    cfg.title = "MeatMon Engine";
    cfg.logicalW = 320;   // 20 x 16px tiles
    cfg.logicalH = 192;   // 12 x 16px tiles
    cfg.windowScale = 3;

    App app;
    if (!app.init(cfg)) return 1;

    DemoGame game(gameDir, selftest);
    int rc = app.run(game, selftest ? 600 : -1);

    if (selftest) {
        bool ok = rc == 0 && game.sawBattleMove();
        std::puts(ok ? "[selftest] OK" : "[selftest] FAILED");
        return ok ? 0 : 1;
    }
    return rc;
}
