#pragma once
#include <cstdint>

namespace render {
// Геометрия в экранных пикселях, источник в текселях; общий CPU-контракт с QuadRenderer.
struct Quad {
    float x = 0, y = 0, w = 0, h = 0;
    float tx = 0, ty = 0, tw = 0, th = 0;
    uint32_t rgba = 0xffffffffu;
    uint32_t flip = 0;
};
static_assert(sizeof(Quad) == 40);
struct QuadRun {
    uint32_t first = 0, count = 0, texture = 0;
};
}
