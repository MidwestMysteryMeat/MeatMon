#pragma once
#include <SDL3/SDL.h>
#include <filesystem>
#include <vector>

namespace mm {

struct Texture {
    SDL_Texture* handle = nullptr;
    int w = 0, h = 0;
};

// Multi-frame sprite: all frames of a GIF (with per-frame delays) or a
// single PNG (one frame, delay 0).
struct SpriteFrames {
    std::vector<SDL_Texture*> frames;
    std::vector<int> delaysMs;
    int w = 0, h = 0;
};

// Load PNG/GIF-first-frame into out. On success any previous handle in out
// is destroyed and replaced (hot-reload keeps Texture* stable for holders).
bool loadTexture(SDL_Renderer* renderer, const std::filesystem::path& path, Texture& out);

// Load PNG (1 frame) or GIF (all frames + delays).
bool loadSpriteFrames(SDL_Renderer* renderer, const std::filesystem::path& path, SpriteFrames& out);

void destroy(Texture& t);
void destroy(SpriteFrames& s);

} // namespace mm
