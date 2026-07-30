#include "meatmon/texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#include <stb_image.h>

#include <algorithm>
#include <fstream>

namespace mm {

namespace {

SDL_Texture* makeTexture(SDL_Renderer* r, const unsigned char* rgba, int w, int h) {
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) return nullptr;
    SDL_UpdateTexture(tex, nullptr, rgba, w * 4);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    return tex;
}

} // namespace

bool loadTexture(SDL_Renderer* renderer, const std::filesystem::path& path, Texture& out) {
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &w, &h, &comp, 4);
    if (!pixels) {
        SDL_Log("loadTexture: cannot decode %s (%s)", path.string().c_str(),
                stbi_failure_reason());
        return false;
    }
    SDL_Texture* tex = makeTexture(renderer, pixels, w, h);
    stbi_image_free(pixels);
    if (!tex) {
        SDL_Log("loadTexture: SDL_CreateTexture failed: %s", SDL_GetError());
        return false;
    }
    if (out.handle) SDL_DestroyTexture(out.handle);
    out = {tex, w, h};
    return true;
}

bool loadSpriteFrames(SDL_Renderer* renderer, const std::filesystem::path& path,
                      SpriteFrames& out) {
    if (path.extension() != ".gif") {
        Texture t;
        if (!loadTexture(renderer, path, t)) return false;
        destroy(out);
        out.frames = {t.handle};
        out.delaysMs = {0};
        out.w = t.w;
        out.h = t.h;
        return true;
    }

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());

    int* delays = nullptr;
    int w = 0, h = 0, frames = 0, comp = 0;
    stbi_uc* data = stbi_load_gif_from_memory(buf.data(), static_cast<int>(buf.size()),
                                              &delays, &w, &h, &frames, &comp, 4);
    if (!data) {
        SDL_Log("loadSpriteFrames: cannot decode %s (%s)", path.string().c_str(),
                stbi_failure_reason());
        return false;
    }

    SpriteFrames result;
    result.w = w;
    result.h = h;
    for (int i = 0; i < frames; ++i) {
        SDL_Texture* tex = makeTexture(renderer,
                                       data + static_cast<size_t>(i) * w * h * 4, w, h);
        if (!tex) break;
        result.frames.push_back(tex);
        // stb reports GIF delays in ms; clamp browser-style tiny delays.
        result.delaysMs.push_back(std::max(20, delays ? delays[i] : 100));
    }
    stbi_image_free(data);
    stbi_image_free(delays);

    if (result.frames.empty()) return false;
    destroy(out);
    out = std::move(result);
    return true;
}

void destroy(Texture& t) {
    if (t.handle) SDL_DestroyTexture(t.handle);
    t = {};
}

void destroy(SpriteFrames& s) {
    for (SDL_Texture* f : s.frames) {
        if (f) SDL_DestroyTexture(f);
    }
    s = {};
}

} // namespace mm
