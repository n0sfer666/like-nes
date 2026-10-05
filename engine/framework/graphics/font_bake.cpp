#include "font_bake.hpp"

#include <algorithm>
#include <cstdio>

#include "font_read.hpp"
#include "section_bake.hpp"

namespace framework::graphics {
namespace {

bool refuse(std::string& error, const std::string& message) {
    error = message;
    return false;
}

std::string hex(uint32_t cp) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "U+%04X", cp);
    return buf;
}

FontGlyph row_of(const FontGlyphSrc& g) { return FontGlyph{g.codepoint, g.x, g.y, g.w, g.advance, 0}; }

bool check_font(const FontSrc& f, std::string& error) {
    const std::string who = "font '" + f.name + "'";
    if (f.line_height == 0 || f.line_height > MAX_FONT_LINE)
        return refuse(error, who + " has a line height of " + std::to_string(f.line_height) + "; allowed 1 to 64");
    if (f.page_w == 0 || f.page_h == 0) return refuse(error, who + " has an empty page");
    if (f.glyphs.empty() || f.glyphs.size() > MAX_FONT_GLYPHS)
        return refuse(error, who + " has " + std::to_string(f.glyphs.size()) + " glyphs; allowed 1 to 4096");
    bool fallback = false;
    for (std::size_t i = 0; i < f.glyphs.size(); ++i) {
        const FontGlyphSrc& g = f.glyphs[i];
        const std::string glyph = who + " glyph " + hex(g.codepoint);
        if (!font_codepoint_ok(g.codepoint)) return refuse(error, glyph + " is a control or not a code point");
        if (!font_glyph_ok(row_of(g), f.line_height, f.page_w, f.page_h))
            return refuse(error, glyph + " has no advance or lies outside its page");
        for (std::size_t j = 0; j < i; ++j)
            if (f.glyphs[j].codepoint == g.codepoint) return refuse(error, glyph + " is declared twice");
        fallback = fallback || g.codepoint == FONT_FALLBACK;
    }
    if (!fallback) return refuse(error, who + " has no '?' glyph to draw unknown characters with");
    return true;
}

void font_of(core::SectionBuilder& b, const FontSrc& f, FontRow& row) {
    std::vector<FontGlyph> glyphs;
    for (const FontGlyphSrc& g : f.glyphs) glyphs.push_back(row_of(g));
    std::sort(glyphs.begin(), glyphs.end(),
              [](const FontGlyph& a, const FontGlyph& c) { return a.codepoint < c.codepoint; });
    row.name_offset = b.text(f.name);
    row.line_height = f.line_height;
    row.texture_guid = f.texture_guid;
    row.glyph_offset = b.block(glyphs.data(), glyphs.size() * sizeof(FontGlyph), alignof(FontGlyph));
    row.glyph_count = static_cast<uint32_t>(glyphs.size());
    row.page_w = f.page_w;
    row.page_h = f.page_h;
}

} // namespace

bool check_fonts(std::span<const FontSrc> fonts, std::string& error) {
    if (fonts.empty()) return refuse(error, "no font to bake");
    for (std::size_t i = 0; i < fonts.size(); ++i) {
        if (fonts[i].name.empty()) return refuse(error, "a font needs a name");
        for (std::size_t j = 0; j < i; ++j)
            if (fonts[j].name == fonts[i].name)
                return refuse(error, "font '" + fonts[i].name + "' is declared twice");
        if (!check_font(fonts[i], error)) return false;
    }
    return true;
}

bool bake_fonts(std::span<const FontSrc> fonts, std::vector<uint8_t>& out, std::string& error) {
    if (!check_fonts(fonts, error)) return false;
    std::vector<FontRow> rows(fonts.size(), FontRow{});
    core::SectionBuilder b(rows.size() * sizeof(FontRow));
    for (std::size_t i = 0; i < fonts.size(); ++i) font_of(b, fonts[i], rows[i]);
    if (!b.finish(FONT_MAGIC, FONT_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), out, error))
        return false;
    FontTable probe;
    if (!probe.open(out.data(), out.size())) return refuse(error, "baked font table fails its own reader");
    return true;
}

} // namespace framework::graphics
