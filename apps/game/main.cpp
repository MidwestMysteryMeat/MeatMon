// MeatMon demo app: tilemap + grid-stepped player with interpolated render,
// PokeAPI-layout sprite resolution, hot-reload, and a battle-engine exchange
// on demand (B). --selftest runs 4 seconds headless-ish and exits 0/1.

#include <meatmon/app.hpp>
#include <meatmon/assets.hpp>
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
        monFront_ = assets_->texture(spriteLib_->resolve({.slug = "puddlit"}).string());
        monBack_ = assets_->texture(
            spriteLib_->resolve({.slug = "emberling", .back = true}).string());

        try {
            dex_ = std::make_unique<battle::Dex>(battle::Dex::load(gameDir_ / "data"));
        } catch (const std::exception& e) {
            SDL_Log("Dex load failed: %s", e.what());
            return false;
        }

        const float ts = static_cast<float>(map_.tileSize);
        player_.curX = player_.prevX = player_.tx * ts;
        player_.curY = player_.prevY = player_.ty * ts;

        std::puts("[MeatMon] arrows/WASD move | B = battle turn | Esc = quit");
        std::puts("[MeatMon] edit game/assets/*.png or game/maps/demo.json while running to hot-reload");
        return tileset_ != nullptr && playerTex_ != nullptr;
    }

    void handleEvent(const SDL_Event& ev) override {
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
            if (ev.key.key == SDLK_ESCAPE) app_->quit();
            if (ev.key.key == SDLK_B) battleTurn();
        }
    }

    void update(double dt) override {
        ++ticks_;
        double now = ticks_ / 60.0;
        assets_->pollHotReload(now);
        pollMapReload();

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

        if (selftest_ && ticks_ == 30) battleTurn();
    }

    void render(SDL_Renderer* r, float alpha) override {
        SDL_SetRenderDrawColor(r, 16, 20, 32, 255);
        SDL_RenderClear(r);

        if (tileset_) map_.draw(r, *tileset_, 0.f, 0.f);

        if (playerTex_) {
            float x = player_.prevX + (player_.curX - player_.prevX) * alpha;
            float y = player_.prevY + (player_.curY - player_.prevY) * alpha;
            SDL_FRect dst{x, y, 16.f, 16.f};
            SDL_RenderTexture(r, playerTex_->handle, nullptr, &dst);
        }

        // Battle preview corners: our lead's back sprite, opponent's front.
        if (monBack_) {
            SDL_FRect dst{6.f, 192.f - 54.f, 48.f, 48.f};
            SDL_RenderTexture(r, monBack_->handle, nullptr, &dst);
        }
        if (monFront_) {
            SDL_FRect dst{320.f - 54.f, 6.f, 48.f, 48.f};
            SDL_RenderTexture(r, monFront_->handle, nullptr, &dst);
        }
    }

private:
    void startBattle() {
        battle_ = std::make_unique<battle::Battle>(*dex_, battle::Format{}, 0x1234ABCDULL);
        battle_->setPlayer(0, "Player",
                           {{.species = "emberling", .moves = {"ember", "tackle", "quickattack"}}});
        battle_->setPlayer(1, "Rival",
                           {{.species = "puddlit", .moves = {"watergun", "tackle"}}});
        battle_->start();
        logCursor_ = 0;
        flushLog();
    }

    void battleTurn() {
        if (!battle_ || battle_->ended()) startBattle();
        for (int s = 0; s < 2; ++s) {
            auto req = battle_->request(s);
            if (req.kind == battle::Request::Kind::Move) {
                int idx = 0;
                for (int i = 0; i < static_cast<int>(req.moves.size()); ++i) {
                    if (req.moves[i].pp > 0) { idx = i; break; }
                }
                battle_->choose(s, {battle::ChoiceKind::Move, idx});
            } else if (req.kind == battle::Request::Kind::Switch && !req.switches.empty()) {
                battle_->choose(s, {battle::ChoiceKind::Switch, req.switches.front()});
            }
        }
        battle_->commitTurn();
        flushLog();
    }

    void flushLog() {
        const auto& lg = battle_->log();
        for (; logCursor_ < lg.size(); ++logCursor_) {
            std::puts(lg[logCursor_].c_str());
            if (lg[logCursor_].rfind("|move|", 0) == 0) sawBattleMove_ = true;
        }
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
    Texture* monFront_ = nullptr;
    Texture* monBack_ = nullptr;
    Player player_;

    std::unique_ptr<battle::Dex> dex_;
    std::unique_ptr<battle::Battle> battle_;
    size_t logCursor_ = 0;
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
    int rc = app.run(game, selftest ? 240 : -1);

    if (selftest) {
        bool ok = rc == 0 && game.sawBattleMove();
        std::puts(ok ? "[selftest] OK" : "[selftest] FAILED");
        return ok ? 0 : 1;
    }
    return rc;
}
