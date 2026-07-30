#pragma once
#include "texture.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace mm {

// A placed map entity (npc, trainer, item, building, warp, trigger).
// The engine parses the common fields; game-specific fields (dialogue,
// trainer teams, warp targets, ...) stay in `extra` for the app/scripts.
struct MapEntity {
    std::string type;                      // "npc", "trainer", "warp", ...
    std::string id;
    std::string sprite;                    // SpriteLibrary slug
    std::string facing = "down";
    int x = 0, y = 0;                      // tile position
    nlohmann::json extra;                  // the full original JSON object
};

// JSON tilemap. Layers are row-major tile indices into a single-row tileset
// image; 0 = empty, 1 = first tile. "collision" is 0/1.
struct Tilemap {
    int width = 0, height = 0, tileSize = 16;
    std::string tilesetPath;               // relative to assets root
    std::vector<int> ground, objects, collision;
    std::vector<MapEntity> entities;

    static bool load(const std::filesystem::path& file, Tilemap& out);

    bool solid(int tx, int ty) const;      // out of bounds = solid
    void draw(SDL_Renderer* renderer, const Texture& tileset,
              float camX, float camY) const;
};

} // namespace mm
