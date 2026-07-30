#pragma once
#include <filesystem>
#include <string>

namespace mm {

struct SpriteQuery {
    std::string slug;        // custom art folder name (checked first)
    int num = 0;             // PokeAPI dex number (checked second)
    bool back = false;
    bool shiny = false;
    bool female = false;
    bool animated = false;   // PokeAPI gen-5 animated GIFs
};

// Maps a species to an image path. Search order:
//   1. game/assets/custom/<slug>/{front|back}[_shiny][_female].{gif,png}
//   2. game/assets/pokemon/sprites/pokemon/[versions/generation-v/
//      black-white/animated/][back/][shiny/][female/]<num>.{gif,png}
// Drop-in folders: no code or data changes needed for new art.
class SpriteLibrary {
public:
    explicit SpriteLibrary(std::filesystem::path assetsRoot)
        : root_(std::move(assetsRoot)) {}

    // Path relative to the assets root, or empty if nothing matches.
    std::filesystem::path resolve(const SpriteQuery& q) const;

private:
    std::filesystem::path root_;
};

} // namespace mm
