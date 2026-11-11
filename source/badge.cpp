#include "badge.h"

#include <algorithm>
#include <cmath>
#include <algorithm>

namespace badge {
namespace {

constexpr int kHeader = 18;
constexpr int kGreen[3] = {34, 197, 94};  // green-500, as RGB
constexpr int kWhite[3] = {255, 255, 255};

struct Tga {
    int w = 0, h = 0, bytes = 0;
    bool topDown = false;
    std::size_t pixels = 0;  // offset of the first pixel
};

bool inspect(const std::vector<std::uint8_t>& f, Tga& t) {
    if (f.size() < kHeader) return false;
    const int idLen = f[0];
    const int cmapType = f[1];
    const int imgType = f[2];
    const int bpp = f[16];
    t.w = f[12] | (f[13] << 8);
    t.h = f[14] | (f[15] << 8);
    t.topDown = (f[17] & 0x20) != 0;
    t.bytes = bpp / 8;
    if (cmapType != 0 || imgType != 2 || (bpp != 24 && bpp != 32) || t.w <= 0 || t.h <= 0) return false;
    t.pixels = std::size_t(kHeader) + idLen;
    return f.size() >= t.pixels + std::size_t(t.w) * t.h * t.bytes;
}

std::size_t pixelIndex(const Tga& t, int x, int y) {
    const int row = t.topDown ? y : t.h - 1 - y;
    return t.pixels + (std::size_t(row) * t.w + x) * t.bytes;
}

// Distance from point p to segment ab.
double segDist(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay;
    const double t = std::clamp(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

bool near(int a, int b) { return std::abs(a - b) <= 12; }

}  // namespace

Geometry geometryFor(int width) {
    Geometry g;
    g.radius = int(width * 0.17);
    const int inset = int(width * 0.04);
    g.cx = width - inset - g.radius;
    g.cy = inset + g.radius;
    g.stroke = std::max(2, int(width * 0.045));
    return g;
}

bool decode(const std::vector<std::uint8_t>& file, std::vector<std::uint8_t>& rgba, int& width, int& height) {
    Tga t;
    if (!inspect(file, t)) return false;
    width = t.w;
    height = t.h;
    rgba.assign(std::size_t(t.w) * t.h * 4, 255);
    for (int y = 0; y < t.h; ++y) {
        for (int x = 0; x < t.w; ++x) {
            const std::uint8_t* s = &file[pixelIndex(t, x, y)];
            std::uint8_t* d = &rgba[(std::size_t(y) * t.w + x) * 4];
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d[3] = t.bytes == 4 ? s[3] : 255;
        }
    }
    return true;
}

bool apply(std::vector<std::uint8_t>& file) {
    Tga t;
    if (!inspect(file, t)) return false;
    const Geometry g = geometryFor(t.w);
    const double r = g.radius;
    const double p1x = g.cx - r * 0.65, p1y = g.cy + r * 0.10;
    const double p2x = g.cx - r * 0.05, p2y = g.cy + r * 0.58;
    const double p3x = g.cx + r * 0.65, p3y = g.cy - r * 0.58;
    const double half = g.stroke / 2.0;
    constexpr int kSamples = 4;

    const int x0 = std::max(0, g.cx - g.radius - 1), x1 = std::min(t.w - 1, g.cx + g.radius + 1);
    const int y0 = std::max(0, g.cy - g.radius - 1), y1 = std::min(t.h - 1, g.cy + g.radius + 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int green = 0, white = 0;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const double px = x + (sx + 0.5) / kSamples, py = y + (sy + 0.5) / kSamples;
                    if (std::hypot(px - g.cx, py - g.cy) > r) continue;
                    const bool check = segDist(px, py, p1x, p1y, p2x, p2y) <= half ||
                                       segDist(px, py, p2x, p2y, p3x, p3y) <= half;
                    (check ? white : green)++;
                }
            }
            if (!green && !white) continue;
            const double gw = green / double(kSamples * kSamples);
            const double ww = white / double(kSamples * kSamples);
            std::uint8_t* d = &file[pixelIndex(t, x, y)];
            for (int c = 0; c < 3; ++c) {  // d is BGR(A)
                const double src = d[2 - c];
                d[2 - c] = std::uint8_t(std::lround(src * (1 - gw - ww) + kGreen[c] * gw + kWhite[c] * ww));
            }
            if (t.bytes == 4) d[3] = std::uint8_t(std::lround(d[3] + (255 - d[3]) * (gw + ww)));
        }
    }
    return true;
}

bool isMarked(const std::vector<std::uint8_t>& file) {
    Tga t;
    if (!inspect(file, t)) return false;
    const Geometry g = geometryFor(t.w);
    // Two spots inside the disc that the checkmark never covers.
    const int spots[2][2] = {{g.cx, g.cy - g.radius / 2}, {g.cx + g.radius / 2, g.cy + g.radius / 2}};
    for (const auto& s : spots) {
        const std::uint8_t* p = &file[pixelIndex(t, s[0], s[1])];
        if (!(near(p[2], kGreen[0]) && near(p[1], kGreen[1]) && near(p[0], kGreen[2]))) return false;
    }
    return true;
}

}  // namespace badge
