#include "meatmon/app.hpp"

namespace mm {

bool App::init(const AppConfig& cfg) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    win_ = SDL_CreateWindow(cfg.title.c_str(),
                            cfg.logicalW * cfg.windowScale,
                            cfg.logicalH * cfg.windowScale,
                            SDL_WINDOW_RESIZABLE);
    if (!win_) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    ren_ = SDL_CreateRenderer(win_, nullptr);
    if (!ren_) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderLogicalPresentation(ren_, cfg.logicalW, cfg.logicalH,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderVSync(ren_, 1);
    return true;
}

int App::run(IGame& game, int maxFrames) {
    if (!game.init(*this)) return 1;

    constexpr double tick = 1.0 / 60.0;   // fixed simulation step
    double acc = 0.0;
    uint64_t prev = SDL_GetTicksNS();
    int frames = 0;
    running_ = true;

    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running_ = false;
            game.handleEvent(ev);
        }

        uint64_t now = SDL_GetTicksNS();
        double frame = static_cast<double>(now - prev) / 1e9;
        prev = now;
        if (frame > 0.25) frame = 0.25;   // debugger/stall clamp
        acc += frame;

        while (acc >= tick) {
            game.update(tick);
            acc -= tick;
        }

        game.render(ren_, static_cast<float>(acc / tick));
        SDL_RenderPresent(ren_);

        if (maxFrames > 0 && ++frames >= maxFrames) running_ = false;
    }
    return 0;
}

App::~App() {
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
    SDL_Quit();
}

} // namespace mm
