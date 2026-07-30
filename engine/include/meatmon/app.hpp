#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace mm {

struct AppConfig {
    std::string title = "MeatMon";
    int logicalW = 320;              // logical pixels, integer-scaled
    int logicalH = 192;
    int windowScale = 3;
};

// Implemented by the app. update() runs at a fixed 60 Hz; render() runs once
// per display frame with alpha = fraction of the next tick already elapsed,
// for interpolated drawing.
class IGame {
public:
    virtual ~IGame() = default;
    virtual bool init(class App& app) = 0;
    virtual void handleEvent(const SDL_Event& ev) = 0;
    virtual void update(double dt) = 0;
    virtual void render(SDL_Renderer* renderer, float alpha) = 0;
};

class App {
public:
    ~App();

    bool init(const AppConfig& cfg);

    // maxFrames > 0 exits after that many display frames (selftest/CI).
    int run(IGame& game, int maxFrames = -1);

    void quit() { running_ = false; }
    SDL_Renderer* renderer() const { return ren_; }
    SDL_Window* window() const { return win_; }

private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    bool running_ = false;
};

} // namespace mm
