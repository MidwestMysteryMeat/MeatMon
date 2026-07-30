#pragma once
#include "texture.hpp"

#include <string>

namespace mm {

// Fixed-cell ASCII bitmap font: chars 32..127 laid out in a 16x6 grid.
// The atlas is plain white glyphs; draw() tints via color mod.
struct Font {
    Texture* tex = nullptr;
    int cellW = 8;
    int cellH = 14;

    void draw(SDL_Renderer* r, const std::string& text, float x, float y,
              SDL_Color color = {255, 255, 255, 255}) const;
    float width(const std::string& text) const {
        return static_cast<float>(text.size() * cellW);
    }
};

} // namespace mm
