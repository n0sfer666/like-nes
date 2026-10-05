#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "font_format.hpp"

namespace framework::graphics {

struct FontGlyphSrc {
    uint32_t codepoint = 0;
    uint16_t x = 0, y = 0;
    uint8_t w = 0, advance = 0;
};

struct FontSrc {
    std::string name;
    uint16_t line_height = 0;
    uint64_t texture_guid = 0;
    uint16_t page_w = 0, page_h = 0;
    std::vector<FontGlyphSrc> glyphs;
};

bool check_fonts(std::span<const FontSrc> fonts, std::string& error);
bool bake_fonts(std::span<const FontSrc> fonts, std::vector<uint8_t>& out, std::string& error);

} // namespace framework::graphics
