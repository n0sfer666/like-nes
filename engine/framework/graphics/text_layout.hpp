#pragma once
#include <cstdint>
#include <span>
#include <string_view>

#include "font_read.hpp"

namespace framework::graphics {

struct GlyphPlace {
    int32_t x = 0;
    int32_t y = 0;
    const FontGlyph* glyph = nullptr;
};

struct TextStats {
    uint32_t placed = 0;
    uint32_t dropped = 0;
    uint32_t unknown = 0;
    uint32_t lines = 0;
    uint32_t width = 0;
};

TextStats layout_text(const FontView& font, std::string_view text, uint32_t max_width, std::span<GlyphPlace> out);

} // namespace framework::graphics
