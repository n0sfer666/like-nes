#include "text_layout.hpp"

#include <algorithm>

#include "utf8_decode.hpp"

namespace framework::graphics {
namespace {

const FontGlyph* glyph_of(const FontView& font, uint32_t cp, TextStats& st) {
    const FontGlyph* g = cp == core::UTF8_INVALID ? nullptr : font_glyph(font, cp);
    if (g != nullptr) return g;
    ++st.unknown;
    return font_glyph(font, FONT_FALLBACK);
}

} // namespace

TextStats layout_text(const FontView& font, std::string_view text, uint32_t max_width, std::span<GlyphPlace> out) {
    TextStats st;
    if (font.row == nullptr || text.empty()) return st;
    const uint32_t line = font.row->line_height;
    st.lines = 1;
    uint32_t x = 0;
    std::size_t at = 0;
    uint32_t cp = 0;
    while (core::utf8_next(text, at, cp)) {
        if (cp == '\n') {
            ++st.lines;
            x = 0;
            continue;
        }
        const FontGlyph* g = glyph_of(font, cp, st);
        if (g == nullptr) continue;
        if (max_width != 0 && x != 0 && x + g->w > max_width) {
            ++st.lines;
            x = 0;
        }
        if (g->w != 0) {
            if (st.placed < out.size())
                out[st.placed++] = GlyphPlace{static_cast<int32_t>(x), static_cast<int32_t>((st.lines - 1) * line), g};
            else
                ++st.dropped;
            st.width = std::max(st.width, x + g->w);
        }
        x += g->advance;
    }
    return st;
}

} // namespace framework::graphics
