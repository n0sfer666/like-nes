#include "art.hpp"

#include <cmath>

namespace game {
namespace {

constexpr uint32_t AW = 192, AH = 128;
constexpr uint32_t SHIP = 128, STAR = 64;

struct Rgba { uint8_t r, g, b, a; };

void put(std::vector<uint8_t>& px, uint32_t x, uint32_t y, Rgba c) {
    uint8_t* p = &px[(y * AW + x) * 4];
    p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}

float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

bool in_tri(float px, float py, float ax, float ay, float bx, float by, float cx, float cy) {
    const float d0 = edge(ax, ay, bx, by, px, py);
    const float d1 = edge(bx, by, cx, cy, px, py);
    const float d2 = edge(cx, cy, ax, ay, px, py);
    const bool neg = (d0 < 0) || (d1 < 0) || (d2 < 0);
    const bool pos = (d0 > 0) || (d1 > 0) || (d2 > 0);
    return !(neg && pos);
}

void draw_ship(std::vector<uint8_t>& px) {
    const Rgba wing{104, 118, 150, 255};
    const Rgba hull{188, 198, 212, 255};
    const Rgba dark{72, 82, 104, 255};
    const Rgba glass{96, 224, 255, 255};
    const Rgba fire{255, 150, 54, 255};
    for (uint32_t y = 0; y < SHIP; ++y)
        for (uint32_t x = 0; x < SHIP; ++x) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            Rgba c{0, 0, 0, 0};
            if (in_tri(fx, fy, 70, 52, 18, 22, 46, 54) ||
                in_tri(fx, fy, 70, 76, 18, 106, 46, 74))
                c = wing;
            if (in_tri(fx, fy, 120, 64, 28, 46, 28, 82))
                c = hull;
            if (fx >= 10 && fx <= 30 && fy >= 50 && fy <= 78)
                c = dark;
            const float cd = std::hypot(fx - 86, fy - 62);
            if (cd <= 7.0f) c = glass;
            if (fx >= 4 && fx <= 12 && fy >= 56 && fy <= 72)
                c = fire;
            if (c.a) put(px, x, y, c);
        }
}

void draw_star(std::vector<uint8_t>& px) {
    const float cx = STAR * 0.5f, cy = STAR * 0.5f, r = 22.0f;
    for (uint32_t y = 0; y < STAR; ++y)
        for (uint32_t x = 0; x < STAR; ++x) {
            const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
            const float k = d >= r ? 0.0f : 1.0f - d / r;
            const uint8_t a = (uint8_t)(std::pow(k, 1.8f) * 255.0f);
            put(px, SHIP + x, y, Rgba{255, 255, 255, a});
        }
}

} // namespace

void set_regions(Atlas& atlas) {
    atlas.w = AW; atlas.h = AH;
    const float iu = 0.5f / AW, iv = 0.5f / AH;
    atlas.ship = {iu, iv, (float)SHIP / AW - iu, (float)SHIP / AH - iv};
    atlas.star = {(float)SHIP / AW + iu, iv, (float)(SHIP + STAR) / AW - iu, (float)STAR / AH - iv};
}

Atlas build_atlas() {
    Atlas atlas;
    set_regions(atlas);
    atlas.px.assign((size_t)AW * AH * 4, 0);
    draw_ship(atlas.px);
    draw_star(atlas.px);
    return atlas;
}

} // namespace game
