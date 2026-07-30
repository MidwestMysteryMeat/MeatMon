#include "meatmon/font.hpp"

namespace mm {

void Font::draw(SDL_Renderer* r, const std::string& text, float x, float y,
                SDL_Color color) const {
    if (!tex || !tex->handle) return;
    SDL_SetTextureColorMod(tex->handle, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(tex->handle, color.a);

    float cx = x;
    for (unsigned char c : text) {
        if (c == '\n') {
            y += static_cast<float>(cellH);
            cx = x;
            continue;
        }
        if (c < 32 || c > 127) c = '?';
        int idx = c - 32;
        SDL_FRect src{static_cast<float>((idx % 16) * cellW),
                      static_cast<float>((idx / 16) * cellH),
                      static_cast<float>(cellW), static_cast<float>(cellH)};
        SDL_FRect dst{cx, y, static_cast<float>(cellW), static_cast<float>(cellH)};
        SDL_RenderTexture(r, tex->handle, &src, &dst);
        cx += static_cast<float>(cellW);
    }
    SDL_SetTextureColorMod(tex->handle, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex->handle, 255);
}

std::string wrapText(std::string text, size_t width) {
    size_t lineStart = 0;
    size_t lastSpace = std::string::npos;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            lineStart = i + 1;
            lastSpace = std::string::npos;
            continue;
        }
        if (text[i] == ' ') lastSpace = i;
        if (i - lineStart >= width && lastSpace != std::string::npos &&
            lastSpace > lineStart) {
            text[lastSpace] = '\n';
            lineStart = lastSpace + 1;
            lastSpace = std::string::npos;
        }
    }
    return text;
}

} // namespace mm
