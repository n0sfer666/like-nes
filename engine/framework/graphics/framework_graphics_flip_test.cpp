#include <cstdio>

#include "camera.hpp"
#include "graphics_sprite.hpp"
#include "platform_args.hpp"
#include "sprite_flip.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

struct Orientation {
    uint8_t flip;
    const char* name;
    uint32_t source[9];
};

// Таблицы написаны руками по документации Tiled («сначала диагональ, затем H, затем V»), а не
// выведены из реализации: D+H — поворот на 90° по часовой, D+V — против, D+H+V — антитранспонирование.
constexpr Orientation ORIENTATIONS[8] = {
    {0, "identity", {0, 1, 2, 3, 4, 5, 6, 7, 8}},
    {SPRITE_FLIP_H, "h", {2, 1, 0, 5, 4, 3, 8, 7, 6}},
    {SPRITE_FLIP_V, "v", {6, 7, 8, 3, 4, 5, 0, 1, 2}},
    {SPRITE_FLIP_H | SPRITE_FLIP_V, "hv", {8, 7, 6, 5, 4, 3, 2, 1, 0}},
    {SPRITE_FLIP_D, "d", {0, 3, 6, 1, 4, 7, 2, 5, 8}},
    {SPRITE_FLIP_D | SPRITE_FLIP_H, "dh", {6, 3, 0, 7, 4, 1, 8, 5, 2}},
    {SPRITE_FLIP_D | SPRITE_FLIP_V, "dv", {2, 5, 8, 1, 4, 7, 0, 3, 6}},
    {SPRITE_FLIP_D | SPRITE_FLIP_H | SPRITE_FLIP_V, "dhv", {8, 5, 2, 7, 4, 1, 6, 3, 0}},
};

void test_orientations() {
    for (const Orientation& o : ORIENTATIONS) {
        bool ok = true;
        for (uint32_t y = 0; y < 3; ++y)
            for (uint32_t x = 0; x < 3; ++x) {
                const Texel t = flip_source(o.flip, 3, {x, y});
                ok = ok && t.y * 3 + t.x == o.source[y * 3 + x];
            }
        if (!ok) std::printf("  orientation %s\n", o.name);
        check(ok, "flip maps output pixels to the hand-written source table");
    }
}

void test_distinct() {
    uint32_t seen[8] = {};
    for (uint8_t f = 0; f < 8; ++f) {
        const Texel a = flip_source(f, 16, {1, 0});
        const Texel b = flip_source(f, 16, {0, 2});
        seen[f] = (a.y * 16 + a.x) << 16 | (b.y * 16 + b.x);
    }
    bool distinct = true;
    for (int i = 0; i < 8; ++i)
        for (int j = i + 1; j < 8; ++j) distinct = distinct && seen[i] != seen[j];
    check(distinct, "eight flip codes give eight different orientations");
}

void test_parallax_axes() {
    Camera c;
    c.center = {fix32::from_int(100), fix32::from_int(40)};
    const CameraConfig cfg;
    const fix32 half = fix32::from_raw(fix32::ONE / 2);
    const Vec2 one = camera_layer_center(c, cfg, 0, half);
    const Vec2 both = camera_layer_center(c, cfg, 0, Vec2{half, half});
    check(one == both, "scalar parallax equals the same share on both axes");
    const Vec2 split = camera_layer_center(c, cfg, 0, Vec2{half, fix32::from_int(1)});
    check(split.x == fix32::from_int(50) && split.y == fix32::from_int(40),
          "per-axis parallax scales each axis by its own share");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    test_orientations();
    test_distinct();
    test_parallax_axes();
    std::printf("framework-graphics-flip: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
