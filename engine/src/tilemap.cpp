#include "meatmon/tilemap.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

namespace mm {

using nlohmann::json;

bool Tilemap::load(const std::filesystem::path& file, Tilemap& out) {
    std::ifstream f(file);
    if (!f) return false;
    json j = json::parse(f, nullptr, false);
    if (j.is_discarded()) {
        SDL_Log("Tilemap: invalid JSON in %s", file.string().c_str());
        return false;
    }

    Tilemap m;
    m.tileSize = j.value("tile_size", 16);
    m.width = j.value("width", 0);
    m.height = j.value("height", 0);
    m.tilesetPath = j.value("tileset", "");
    if (m.width <= 0 || m.height <= 0) return false;

    auto readLayer = [&](const char* name, std::vector<int>& dst) {
        dst.assign(static_cast<size_t>(m.width) * m.height, 0);
        if (!j.contains("layers") || !j["layers"].contains(name)) return;
        const auto& rows = j["layers"][name];
        for (int y = 0; y < static_cast<int>(rows.size()) && y < m.height; ++y) {
            const auto& row = rows[y];
            for (int x = 0; x < static_cast<int>(row.size()) && x < m.width; ++x) {
                dst[static_cast<size_t>(y) * m.width + x] = row[x].get<int>();
            }
        }
    };
    readLayer("ground", m.ground);
    readLayer("objects", m.objects);
    readLayer("collision", m.collision);

    if (j.contains("entities") && j["entities"].is_array()) {
        for (const auto& e : j["entities"]) {
            MapEntity ent;
            ent.type = e.value("type", "npc");
            ent.id = e.value("id", "");
            ent.sprite = e.value("sprite", "");
            ent.facing = e.value("facing", "down");
            ent.x = e.value("x", 0);
            ent.y = e.value("y", 0);
            ent.extra = e;
            m.entities.push_back(std::move(ent));
        }
    }

    out = std::move(m);
    return true;
}

bool Tilemap::solid(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return true;
    return collision[static_cast<size_t>(ty) * width + tx] != 0;
}

void Tilemap::draw(SDL_Renderer* renderer, const Texture& tileset,
                   float camX, float camY) const {
    const float ts = static_cast<float>(tileSize);
    auto drawLayer = [&](const std::vector<int>& layer) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = layer[static_cast<size_t>(y) * width + x];
                if (idx <= 0) continue;
                SDL_FRect src{static_cast<float>((idx - 1) * tileSize), 0.f, ts, ts};
                SDL_FRect dst{x * ts - camX, y * ts - camY, ts, ts};
                SDL_RenderTexture(renderer, tileset.handle, &src, &dst);
            }
        }
    };
    drawLayer(ground);
    drawLayer(objects);
}

} // namespace mm
