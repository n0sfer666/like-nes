#include "font_import.hpp"

#include <algorithm>
#include <bit>

#include "tmj_doc.hpp"
#include "utf8_decode.hpp"

namespace framework::graphics {
namespace {

struct Mask {
    uint32_t codepoint = 0;
    std::span<const json::Node> rows;
    uint8_t w = 0;
};

bool codepoint_of(const tiled::Doc& d, const json::Node& key, uint32_t& cp, std::string& error) {
    std::size_t at = 0;
    if (!core::utf8_next(key.s, at, cp) || at != key.s.size() || cp == core::UTF8_INVALID)
        return d.fail(key, "a glyph key is exactly one character", error);
    if (!font_codepoint_ok(cp)) return d.fail(key, "a glyph of a control character", error);
    return true;
}

bool mask_of(const tiled::Doc& d, const json::Node& key, const json::Node& value, Mask& out, std::string& error) {
    if (!codepoint_of(d, key, out.codepoint, error)) return false;
    if (value.kind != json::Kind::Array) return d.fail(value, "a glyph is an array of row masks", error);
    out.rows = d.items(value);
    if (out.rows.empty() || out.rows.size() > MAX_FONT_LINE)
        return d.fail(value, "a glyph has 1 to 64 rows", error);
    uint32_t bits = 0;
    for (const json::Node& r : out.rows) {
        if (r.kind != json::Kind::Int || r.i < 0 || r.i > 0xFFFF)
            return d.fail(r, "a row mask is an integer from 0 to 65535", error);
        bits |= static_cast<uint32_t>(r.i);
    }
    out.w = static_cast<uint8_t>(std::bit_width(bits));
    return true;
}

uint8_t mono_of(const std::vector<Mask>& masks) {
    uint8_t ascii = 0, any = 0;
    for (const Mask& m : masks) {
        any = std::max(any, m.w);
        if (m.codepoint < 0x7F) ascii = std::max(ascii, m.w);
    }
    return static_cast<uint8_t>((ascii != 0 ? ascii : any) + 1);
}

void paint(const Mask& m, uint16_t x0, uint16_t y0, uint16_t page_w, std::vector<uint8_t>& rgba) {
    for (std::size_t y = 0; y < m.rows.size(); ++y)
        for (uint16_t x = 0; x < m.w; ++x)
            if ((static_cast<uint64_t>(m.rows[y].i) >> x) & 1u) {
                const std::size_t at = ((y0 + y) * page_w + x0 + x) * 4;
                std::fill_n(rgba.begin() + static_cast<std::ptrdiff_t>(at), 4, uint8_t{255});
            }
}

} // namespace

bool import_bitmask_font(const std::string& name, const std::string& file, std::span<const std::byte> bytes,
                         FontAtlas& out, std::string& error) {
    out = FontAtlas{};
    tiled::Doc d;
    d.file = file;
    d.in = bytes;
    if (!d.load(error)) return false;
    const json::Node& root = d.root();
    if (root.count == 0 || root.count > MAX_FONT_GLYPHS) return d.fail(root, "a font has 1 to 4096 glyphs", error);
    std::vector<Mask> masks(root.count);
    for (uint32_t k = 0; k < root.count; ++k) {
        if (!mask_of(d, d.nodes[root.first + 2 * k], d.nodes[root.first + 2 * k + 1], masks[k], error))
            return false;
        if (masks[k].rows.size() != masks[0].rows.size())
            return d.fail(d.nodes[root.first + 2 * k + 1], "every glyph has as many rows as the first one", error);
    }
    std::sort(masks.begin(), masks.end(), [](const Mask& a, const Mask& b) { return a.codepoint < b.codepoint; });
    const auto line = static_cast<uint16_t>(masks[0].rows.size());
    uint8_t cell = 1;
    for (const Mask& m : masks) cell = std::max(cell, m.w);
    const auto n = static_cast<uint32_t>(masks.size());
    FontSrc& f = out.font;
    f.name = name;
    f.line_height = line;
    f.page_w = static_cast<uint16_t>(std::min<uint32_t>(n, FONT_CELLS_PER_ROW) * cell);
    f.page_h = static_cast<uint16_t>((n + FONT_CELLS_PER_ROW - 1) / FONT_CELLS_PER_ROW * line);
    out.rgba.assign(std::size_t{f.page_w} * f.page_h * 4, 0);
    const uint8_t mono = mono_of(masks);
    for (uint32_t i = 0; i < n; ++i) {
        const auto x = static_cast<uint16_t>(i % FONT_CELLS_PER_ROW * cell);
        const auto y = static_cast<uint16_t>(i / FONT_CELLS_PER_ROW * line);
        const Mask& m = masks[i];
        f.glyphs.push_back(FontGlyphSrc{m.codepoint, x, y, m.w, std::max(mono, static_cast<uint8_t>(m.w + 1))});
        paint(m, x, y, f.page_w, out.rgba);
    }
    if (check_fonts({&f, 1}, error)) return true;
    error = file + ": " + error;
    return false;
}

} // namespace framework::graphics
