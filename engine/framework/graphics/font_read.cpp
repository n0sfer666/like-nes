#include "font_read.hpp"

#include <algorithm>
#include <cstring>

namespace framework::graphics {
namespace {

bool row_ok(const core::SectionView& v, const FontRow& r, uint64_t& cursor) {
    std::span<const FontGlyph> glyphs;
    if (!v.text(r.name_offset) || v.strings[r.name_offset] == '\0' || r.pad0 != 0 || r.pad1 != 0) return false;
    if (r.line_height == 0 || r.line_height > MAX_FONT_LINE || r.page_w == 0 || r.page_h == 0) return false;
    if (r.glyph_count == 0 || r.glyph_count > MAX_FONT_GLYPHS || !v.take(r.glyph_offset, r.glyph_count, glyphs))
        return false;
    if (r.glyph_offset < cursor) return false;
    cursor = uint64_t{r.glyph_offset} + glyphs.size_bytes();
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        if (!font_glyph_ok(glyphs[i], r.line_height, r.page_w, r.page_h)) return false;
        if (i > 0 && glyphs[i - 1].codepoint >= glyphs[i].codepoint) return false;
    }
    return font_glyph(FontView{&r, glyphs}, FONT_FALLBACK) != nullptr;
}

} // namespace

bool font_codepoint_ok(uint32_t cp) {
    return cp >= 0x20 && !(cp >= 0x7F && cp <= 0x9F) && !(cp >= 0xD800 && cp <= 0xDFFF) && cp <= 0x10FFFF;
}

bool font_glyph_ok(const FontGlyph& g, uint16_t line_height, uint16_t page_w, uint16_t page_h) {
    return font_codepoint_ok(g.codepoint) && g.pad == 0 && g.advance > 0 && g.x <= page_w &&
           g.w <= page_w - g.x && g.y <= page_h && line_height <= page_h - g.y;
}

bool FontTable::open(const void* data, std::size_t size) {
    view_ = core::SectionView{};
    rows_ = nullptr;
    core::SectionView v;
    if (!core::open_section(data, size, FONT_MAGIC, FONT_VERSION, sizeof(FontRow), alignof(FontRow), v))
        return false;
    const std::span<const FontRow> rows = v.at<FontRow>(v.header->rows_offset, v.header->count);
    uint64_t cursor = v.rows_end;
    for (const FontRow& r : rows)
        if (!row_ok(v, r, cursor)) return false;
    view_ = v;
    rows_ = rows.data();
    return true;
}

const char* FontTable::name(uint32_t index) const {
    if (index >= count()) return "";
    return view_.strings + rows_[index].name_offset;
}

FontView FontTable::font(uint32_t index) const {
    if (index >= count()) return {};
    const FontRow& r = rows_[index];
    return FontView{&r, view_.at<FontGlyph>(r.glyph_offset, r.glyph_count)};
}

FontView FontTable::find(const char* wanted) const {
    if (wanted == nullptr) return {};
    for (uint32_t i = 0; i < count(); ++i)
        if (std::strcmp(name(i), wanted) == 0) return font(i);
    return {};
}

const FontGlyph* font_glyph(const FontView& font, uint32_t codepoint) {
    const auto it = std::lower_bound(font.glyphs.begin(), font.glyphs.end(), codepoint,
                                     [](const FontGlyph& g, uint32_t cp) { return g.codepoint < cp; });
    return it != font.glyphs.end() && it->codepoint == codepoint ? &*it : nullptr;
}

} // namespace framework::graphics
