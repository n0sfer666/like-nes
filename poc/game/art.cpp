#include "art.hpp"

#include <cmath>

namespace game {
namespace {

constexpr uint32_t AW = 256, AH = 256;
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
    const Rgba wing{104, 118, 150, 255}, hull{188, 198, 212, 255}, dark{72, 82, 104, 255};
    const Rgba glass{96, 224, 255, 255}, fire{255, 150, 54, 255};
    for (uint32_t y = 0; y < SHIP; ++y)
        for (uint32_t x = 0; x < SHIP; ++x) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            Rgba c{0, 0, 0, 0};
            if (in_tri(fx, fy, 70, 52, 18, 22, 46, 54) || in_tri(fx, fy, 70, 76, 18, 106, 46, 74))
                c = wing;
            if (in_tri(fx, fy, 120, 64, 28, 46, 28, 82)) c = hull;
            if (fx >= 10 && fx <= 30 && fy >= 50 && fy <= 78) c = dark;
            if (std::hypot(fx - 86, fy - 62) <= 7.0f) c = glass;
            if (fx >= 4 && fx <= 12 && fy >= 56 && fy <= 72) c = fire;
            if (c.a) put(px, x, y, c);
        }
}

void draw_star(std::vector<uint8_t>& px) {
    const float cx = STAR * 0.5f, cy = STAR * 0.5f, r = 22.0f;
    for (uint32_t y = 0; y < STAR; ++y)
        for (uint32_t x = 0; x < STAR; ++x) {
            const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
            const float k = d >= r ? 0.0f : 1.0f - d / r;
            put(px, SHIP + x, y, Rgba{255, 255, 255, (uint8_t)(std::pow(k, 1.8f) * 255.0f)});
        }
}

// Враг (64x48 @128,64): угловатый истребитель носом ВЛЕВО (к игроку), красный глаз-кокпит.
void draw_enemy(std::vector<uint8_t>& px) {
    const uint32_t ox = 128, oy = 64, W = 64, H = 48;
    const Rgba body{170, 74, 96, 255}, dark{110, 44, 62, 255}, eye{255, 92, 72, 255};
    const Rgba trail{255, 176, 96, 255};
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            Rgba c{0, 0, 0, 0};
            if (in_tri(fx, fy, 8, 24, 52, 6, 52, 42)) c = body;              // корпус (нос влево)
            if (in_tri(fx, fy, 40, 24, 62, 2, 62, 12) ||
                in_tri(fx, fy, 40, 24, 62, 36, 62, 46)) c = dark;           // крылья сзади
            if (fx >= 54 && fx <= 62 && fy >= 20 && fy <= 28) c = trail;     // выхлоп справа
            if (std::hypot(fx - 22, fy - 24) <= 5.0f) c = eye;              // глаз
            if (c.a) put(px, ox + x, oy + y, c);
        }
}

// Снаряд (24x8 @128,112): яркий энерго-болт.
void draw_bullet(std::vector<uint8_t>& px) {
    const uint32_t ox = 128, oy = 112, W = 24, H = 8;
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const float k = 1.0f - std::hypot((x + 0.5f - 12) / 12.0f, (y + 0.5f - 4) / 4.0f);
            if (k <= 0) continue;
            const uint8_t a = (uint8_t)(std::pow(k, 0.7f) * 255.0f);
            put(px, ox + x, oy + y, Rgba{180, 255, 255, a});
        }
}

// Битмап-шрифт 3x5 → цифры 0–9 @ (d*18,128), масштаб 4 (12x20 в ячейке 18x24).
const uint8_t FONT[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, {0b010, 0b110, 0b010, 0b010, 0b111},
    {0b111, 0b001, 0b111, 0b100, 0b111}, {0b111, 0b001, 0b111, 0b001, 0b111},
    {0b101, 0b101, 0b111, 0b001, 0b001}, {0b111, 0b100, 0b111, 0b001, 0b111},
    {0b111, 0b100, 0b111, 0b101, 0b111}, {0b111, 0b001, 0b001, 0b010, 0b010},
    {0b111, 0b101, 0b111, 0b101, 0b111}, {0b111, 0b101, 0b111, 0b001, 0b111},
};

void draw_digits(std::vector<uint8_t>& px) {
    for (uint32_t d = 0; d < 10; ++d)
        for (uint32_t r = 0; r < 5; ++r)
            for (uint32_t c = 0; c < 3; ++c)
                if (FONT[d][r] & (1u << (2 - c)))
                    for (uint32_t sy = 0; sy < 4; ++sy)
                        for (uint32_t sx = 0; sx < 4; ++sx)
                            put(px, d * 18 + 3 + c * 4 + sx, 128 + 2 + r * 4 + sy,
                                Rgba{255, 255, 255, 255});
}

Region rgn(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    const float iu = 0.5f / AW, iv = 0.5f / AH;
    return {x0 / (float)AW + iu, y0 / (float)AH + iv, x1 / (float)AW - iu, y1 / (float)AH - iv};
}

} // namespace

void set_regions(Atlas& atlas) {
    atlas.w = AW; atlas.h = AH;
    atlas.ship = rgn(0, 0, SHIP, SHIP);
    atlas.star = rgn(SHIP, 0, SHIP + STAR, STAR);
    atlas.enemy = rgn(128, 64, 192, 112);
    atlas.bullet = rgn(128, 112, 152, 120);
    for (uint32_t d = 0; d < 10; ++d) atlas.digit[d] = rgn(d * 18, 128, d * 18 + 18, 152);
}

Atlas build_atlas() {
    Atlas atlas;
    set_regions(atlas);
    atlas.px.assign((size_t)AW * AH * 4, 0);
    draw_ship(atlas.px);
    draw_star(atlas.px);
    draw_enemy(atlas.px);
    draw_bullet(atlas.px);
    draw_digits(atlas.px);
    return atlas;
}

} // namespace game
