#pragma once
#include "texture.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace mm {

// Caches textures/sprites by path relative to the assets root and polls
// file mtimes for hot-reload. Returned pointers stay valid for the manager's
// lifetime; on reload the GPU texture is swapped underneath them.
class AssetManager {
public:
    AssetManager(SDL_Renderer* renderer, std::filesystem::path root)
        : ren_(renderer), root_(std::move(root)) {}
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // nullptr if the file is missing/undecodable (logged, never fatal).
    Texture* texture(const std::string& relPath);
    SpriteFrames* sprite(const std::string& relPath);

    // Call from update(); checks mtimes at most every 0.5 s of nowSeconds.
    void pollHotReload(double nowSeconds);

    const std::filesystem::path& root() const { return root_; }

private:
    struct TexEntry {
        Texture tex;
        std::filesystem::path abs;
        std::filesystem::file_time_type mtime;
    };
    struct SpriteEntry {
        SpriteFrames sprite;
        std::filesystem::path abs;
        std::filesystem::file_time_type mtime;
    };

    SDL_Renderer* ren_;
    std::filesystem::path root_;
    std::unordered_map<std::string, TexEntry> textures_;
    std::unordered_map<std::string, SpriteEntry> sprites_;
    double lastPoll_ = 0.0;
};

} // namespace mm
