#include "meatmon/assets.hpp"

namespace mm {

namespace fs = std::filesystem;

AssetManager::~AssetManager() {
    for (auto& [k, e] : textures_) destroy(e.tex);
    for (auto& [k, e] : sprites_) destroy(e.sprite);
}

Texture* AssetManager::texture(const std::string& relPath) {
    if (relPath.empty()) return nullptr;
    auto it = textures_.find(relPath);
    if (it != textures_.end()) {
        return it->second.tex.handle ? &it->second.tex : nullptr;
    }

    TexEntry e;
    e.abs = root_ / relPath;
    if (!loadTexture(ren_, e.abs, e.tex)) {
        SDL_Log("AssetManager: missing texture %s", e.abs.string().c_str());
    }
    std::error_code ec;
    e.mtime = fs::last_write_time(e.abs, ec);
    auto [ins, ok] = textures_.emplace(relPath, std::move(e));
    return ins->second.tex.handle ? &ins->second.tex : nullptr;
}

SpriteFrames* AssetManager::sprite(const std::string& relPath) {
    if (relPath.empty()) return nullptr;
    auto it = sprites_.find(relPath);
    if (it != sprites_.end()) {
        return it->second.sprite.frames.empty() ? nullptr : &it->second.sprite;
    }

    SpriteEntry e;
    e.abs = root_ / relPath;
    if (!loadSpriteFrames(ren_, e.abs, e.sprite)) {
        SDL_Log("AssetManager: missing sprite %s", e.abs.string().c_str());
    }
    std::error_code ec;
    e.mtime = fs::last_write_time(e.abs, ec);
    auto [ins, ok] = sprites_.emplace(relPath, std::move(e));
    return ins->second.sprite.frames.empty() ? nullptr : &ins->second.sprite;
}

void AssetManager::pollHotReload(double nowSeconds) {
    if (nowSeconds - lastPoll_ < 0.5) return;
    lastPoll_ = nowSeconds;

    std::error_code ec;
    for (auto& [rel, e] : textures_) {
        auto m = fs::last_write_time(e.abs, ec);
        if (ec || m == e.mtime) continue;
        e.mtime = m;
        if (loadTexture(ren_, e.abs, e.tex)) {
            SDL_Log("hot-reloaded %s", rel.c_str());
        }
    }
    for (auto& [rel, e] : sprites_) {
        auto m = fs::last_write_time(e.abs, ec);
        if (ec || m == e.mtime) continue;
        e.mtime = m;
        if (loadSpriteFrames(ren_, e.abs, e.sprite)) {
            SDL_Log("hot-reloaded %s", rel.c_str());
        }
    }
}

} // namespace mm
