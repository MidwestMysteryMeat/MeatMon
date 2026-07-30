#pragma once
#include "texture.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace mm {

// JSON tilemap. Layers are row-major tile indices into a single-row tileset
// image; 0 = empty, 1 = first tile. "collision" is 0/1. warps/events arrays
// are parsed from Phase 2 on.
struct Tilemap {
    int width = 0, height = 0, tileSize = 16;
    std::string tilesetPath;               // relative to assets root
    std::vector<int> ground, objects, collision;

    static bool load(const std::filesystem::path& file, Tilemap& out);

    bool solid(int tx, int ty) const;      // out of bounds = solid
    void draw(SDL_Renderer* renderer, const Texture& tileset,
              float camX, float camY) const;
};

} // namespace mm
