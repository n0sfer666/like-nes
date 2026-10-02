#pragma once
#include <cstdint>

#include "visual_read.hpp"

namespace framework::tilemap {

struct TexelRect {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = 0;
    uint32_t h = 0;
};

// Прямоугольник визуального индекса в текстуре его тайлсета, по раскладке Tiled: колонки слева
// направо, ряды сверху вниз, `margin` от края, `spacing` между тайлами. Индекс вне тайлсетов — отказ.
bool tile_texels(const VisualMap& map, uint16_t index, TexelRect& out);

} // namespace framework::tilemap
