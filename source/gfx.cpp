#include "gfx.h"

#include <SDL2_gfxPrimitives.h>

#include <algorithm>
#include <cstdio>

namespace gfx {

Canvas::Canvas(SDL_Renderer* renderer, std::vector<std::uint8_t> font) : r_(renderer), font_(std::move(font)) {
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
}

Canvas::~Canvas() {
    for (auto& kv : cache_) SDL_DestroyTexture(kv.second.tex);
    for (auto& kv : fonts_) TTF_CloseFont(kv.second);
}

TTF_Font* Canvas::font(Size size) {
    const int px = static_cast<int>(size);
    auto it = fonts_.find(px);
    if (it != fonts_.end()) return it->second;
    TTF_Font* f = nullptr;
    if (!font_.empty()) {
        SDL_RWops* rw = SDL_RWFromConstMem(font_.data(), static_cast<int>(font_.size()));
        f = TTF_OpenFontRW(rw, 1, px);
    }
    fonts_[px] = f;
    return f;
}

void Canvas::clear(Color c) {
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
    SDL_RenderClear(r_);
}

void Canvas::fill(int x, int y, int w, int h, int radius, Color c) {
    if (radius <= 0) {
        SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
        const SDL_Rect rc{x, y, w, h};
        SDL_RenderFillRect(r_, &rc);
        return;
    }
    roundedBoxRGBA(r_, x, y, x + w - 1, y + h - 1, radius, c.r, c.g, c.b, c.a);
}

void Canvas::card(int x, int y, int w, int h, int radius, Color fillColor, int border, Color borderColor) {
    if (border > 0) fill(x, y, w, h, radius, borderColor);
    fill(x + border, y + border, w - 2 * border, h - 2 * border, std::max(0, radius - border), fillColor);
}

void Canvas::circle(int cx, int cy, int radius, Color c) {
    filledCircleRGBA(r_, cx, cy, radius, c.r, c.g, c.b, c.a);
    aacircleRGBA(r_, cx, cy, radius, c.r, c.g, c.b, c.a);
}

void Canvas::line(int x1, int y1, int x2, int y2, int width, Color c) {
    thickLineRGBA(r_, x1, y1, x2, y2, width, c.r, c.g, c.b, c.a);
}

void Canvas::checkBadge(int cx, int cy, int radius, Color disc, Color mark) {
    circle(cx, cy, radius, disc);
    const int w = std::max(2, radius / 4);
    const int x1 = cx - radius * 0.45, y1 = cy + radius * 0.02;
    const int x2 = cx - radius * 0.1, y2 = cy + radius * 0.4;
    const int x3 = cx + radius * 0.5, y3 = cy - radius * 0.38;
    line(x1, y1, x2, y2, w, mark);
    line(x2, y2, x3, y3, w, mark);
    circle(x1, y1, w / 2, mark);
    circle(x3, y3, w / 2, mark);
}

std::string Canvas::fit(const std::string& s, Size size, int maxWidth) {
    if (maxWidth <= 0 || textWidth(s, size) <= maxWidth) return s;
    std::string out = s;
    while (!out.empty()) {
        do {
            out.pop_back();
        } while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80);
        if (textWidth(out + "...", size) <= maxWidth) break;
    }
    // a trailing multi-byte lead byte would be invalid
    return out + "...";
}

int Canvas::textWidth(const std::string& s, Size size) {
    TTF_Font* f = font(size);
    int w = 0, h = 0;
    if (!f || s.empty() || TTF_SizeUTF8(f, s.c_str(), &w, &h) != 0) return 0;
    return w;
}

int Canvas::lineHeight(Size size) {
    TTF_Font* f = font(size);
    return f ? TTF_FontLineSkip(f) : static_cast<int>(size) + 6;
}

int Canvas::text(const std::string& s, Size size, Color c, int x, int y, Align align, int maxWidth) {
    TTF_Font* f = font(size);
    if (!f || s.empty()) return 0;
    const std::string shown = fit(s, size, maxWidth);
    char key[32];
    std::snprintf(key, sizeof key, "%d|%02x%02x%02x|", static_cast<int>(size), c.r, c.g, c.b);
    const std::string k = key + shown;
    auto it = cache_.find(k);
    if (it == cache_.end()) {
        if (cache_.size() > 600) {  // keep memory bounded
            for (auto& kv : cache_) SDL_DestroyTexture(kv.second.tex);
            cache_.clear();
        }
        SDL_Surface* surf = TTF_RenderUTF8_Blended(f, shown.c_str(), SDL_Color{c.r, c.g, c.b, 255});
        if (!surf) return 0;
        Cached cached;
        cached.tex = SDL_CreateTextureFromSurface(r_, surf);
        cached.w = surf->w;
        cached.h = surf->h;
        SDL_FreeSurface(surf);
        it = cache_.emplace(k, cached).first;
    }
    const Cached& t = it->second;
    int dx = x;
    if (align == Align::Center) dx = x - t.w / 2;
    if (align == Align::Right) dx = x - t.w;
    const SDL_Rect dst{dx, y, t.w, t.h};
    SDL_SetTextureAlphaMod(t.tex, c.a);
    SDL_RenderCopy(r_, t.tex, nullptr, &dst);
    return t.w;
}

std::vector<std::string> Canvas::wrap(const std::string& s, Size size, int maxWidth) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&]() {
        if (word.empty()) return;
        const std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(trial, size) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = trial;
        }
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flush();
        else word += ch;
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

SDL_Texture* Canvas::texture(const std::vector<std::uint8_t>& rgba, int w, int h) {
    SDL_Texture* t = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, rgba.data(), w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
    return t;
}

void Canvas::blit(SDL_Texture* t, int x, int y, int w, int h) {
    const SDL_Rect dst{x, y, w, h};
    SDL_RenderCopy(r_, t, nullptr, &dst);
}

}  // namespace gfx
