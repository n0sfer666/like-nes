#pragma once
#include <cstdint>

namespace framework::graphics {

constexpr uint8_t FONT_MAGIC[4] = {'L', 'N', 'F', 'N'};
constexpr uint32_t FONT_VERSION = 1;
constexpr uint16_t MAX_FONT_LINE = 64;
constexpr uint32_t MAX_FONT_GLYPHS = 4096;
constexpr uint32_t FONT_FALLBACK = '?';

struct FontRow {
    uint32_t name_offset;
    uint16_t line_height;
    uint16_t pad0;
    uint64_t texture_guid;
    uint32_t glyph_offset;
    uint32_t glyph_count;
    uint16_t page_w;
    uint16_t page_h;
    uint32_t pad1;
};
static_assert(sizeof(FontRow) == 32, "FontRow layout pinned (zero-parse ABI)");

struct FontGlyph {
    uint32_t codepoint;
    uint16_t x;
    uint16_t y;
    uint8_t w;
    uint8_t advance;
    uint16_t pad;
};
static_assert(sizeof(FontGlyph) == 12, "FontGlyph layout pinned (zero-parse ABI)");

bool font_codepoint_ok(uint32_t cp);

} // namespace framework::graphics
