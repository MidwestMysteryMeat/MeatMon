#include "meatmon/sprites.hpp"

#include <vector>

namespace mm {

namespace fs = std::filesystem;

fs::path SpriteLibrary::resolve(const SpriteQuery& q) const {
    std::vector<fs::path> candidates;

    if (!q.slug.empty()) {
        std::string base = q.back ? "back" : "front";
        if (q.shiny) base += "_shiny";
        if (q.female) base += "_female";
        for (const char* ext : {".gif", ".png"}) {
            candidates.push_back(fs::path("custom") / q.slug / (base + ext));
        }
    }

    if (q.num > 0) {
        fs::path p = fs::path("pokemon") / "sprites" / "pokemon";
        if (q.animated) {
            p = p / "versions" / "generation-v" / "black-white" / "animated";
        }
        if (q.back) p /= "back";
        if (q.shiny) p /= "shiny";
        if (q.female) p /= "female";
        candidates.push_back(p / (std::to_string(q.num) + (q.animated ? ".gif" : ".png")));
    }

    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(root_ / c, ec)) return c;
    }
    return {};
}

} // namespace mm
