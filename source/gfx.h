// SDL2 drawing helpers: colours, rounded shapes, cached text, RGBA textures.
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gfx {

struct Color {
    std::uint8_t r, g, b, a = 255;
};

/// Theme tokens (dark UI; contrast checked against the card and background colours).
namespace theme {
constexpr Color bg{15, 23, 42};          // #0F172A
constexpr Color card{25, 33, 52};        // #192134
constexpr Color raised{34, 48, 74};      // #22304A
constexpr Color line{44, 56, 82};        // border on dark
constexpr Color text{248, 250, 252};     // #F8FAFC
constexpr Color muted{148, 163, 184};    // #94A3B8
constexpr Color green{34, 197, 94};      // #22C55E accent and focus ring
constexpr Color greenDark{21, 128, 61};  // #15803D fill behind white text
constexpr Color amber{245, 158, 11};     // #F59E0B changed rows, warnings
constexpr Color red{239, 68, 68};        // #EF4444 errors
}  // namespace theme

/// Text sizes in pixels at 1280x720.
enum class Size { Small = 20, Body = 24, Large = 32, Title = 48 };

enum class Align { Left, Center, Right };

class Canvas {
public:
    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;

    /// Args:
    ///     renderer: Renderer owned by the caller.
    ///     font: TrueType font bytes; copied and kept alive by the canvas.
    Canvas(SDL_Renderer* renderer, std::vector<std::uint8_t> font);
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    SDL_Renderer* renderer() const { return r_; }
    bool hasFont() const { return !font_.empty(); }

    void clear(Color c);
    void fill(int x, int y, int w, int h, int radius, Color c);
    /// Rounded rectangle with a border of `border` pixels.
    void card(int x, int y, int w, int h, int radius, Color fillColor, int border, Color borderColor);
    void circle(int cx, int cy, int radius, Color c);
    void line(int x1, int y1, int x2, int y2, int width, Color c);
    /// Round badge with a check, used as logo and as status mark.
    void checkBadge(int cx, int cy, int radius, Color disc, Color mark);

    /// Draws text; returns the drawn width. `maxWidth` > 0 shortens the text with "...".
    int text(const std::string& s, Size size, Color c, int x, int y, Align align = Align::Left, int maxWidth = 0);
    int textWidth(const std::string& s, Size size);
    int lineHeight(Size size);
    /// Splits text into lines no wider than maxWidth.
    std::vector<std::string> wrap(const std::string& s, Size size, int maxWidth);

    /// Creates a texture from top-down RGBA pixels. The caller owns it.
    SDL_Texture* texture(const std::vector<std::uint8_t>& rgba, int w, int h);
    void blit(SDL_Texture* t, int x, int y, int w, int h);

    void present() { SDL_RenderPresent(r_); }

private:
    struct Cached {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
    };
    TTF_Font* font(Size size);
    std::string fit(const std::string& s, Size size, int maxWidth);

    SDL_Renderer* r_;
    std::vector<std::uint8_t> font_;
    std::map<int, TTF_Font*> fonts_;
    std::map<std::string, Cached> cache_;
};

}  // namespace gfx
