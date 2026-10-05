#pragma once
#include <cstdint>
#include <span>

#include "quad_batch.hpp"
#include "text_layout.hpp"

namespace framework::graphics {

struct TextPen {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t scale = 1;
    uint32_t rgba = 0xffffffffu;
};

struct TextQuadStats {
    uint32_t quads = 0;
    uint32_t dropped = 0;
};

TextQuadStats text_quads(const FontView& font, std::span<const GlyphPlace> places, const TextPen& pen,
                         std::span<render::Quad> out);

} // namespace framework::graphics
