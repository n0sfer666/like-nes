#include "text_quads.hpp"

namespace framework::graphics {

TextQuadStats text_quads(const FontView& font, std::span<const GlyphPlace> places, const TextPen& pen,
                         std::span<render::Quad> out) {
    TextQuadStats st;
    if (font.row == nullptr || pen.scale == 0) return st;
    const auto scale = static_cast<float>(pen.scale);
    const auto line = static_cast<float>(font.row->line_height);
    for (const GlyphPlace& p : places) {
        if (p.glyph == nullptr) continue;
        if (st.quads == out.size()) {
            ++st.dropped;
            continue;
        }
        render::Quad& q = out[st.quads++];
        q = render::Quad{};
        q.x = static_cast<float>(pen.x) + static_cast<float>(p.x) * scale;
        q.y = static_cast<float>(pen.y) + static_cast<float>(p.y) * scale;
        q.w = static_cast<float>(p.glyph->w) * scale;
        q.h = line * scale;
        q.tx = static_cast<float>(p.glyph->x);
        q.ty = static_cast<float>(p.glyph->y);
        q.tw = static_cast<float>(p.glyph->w);
        q.th = line;
        q.rgba = pen.rgba;
    }
    return st;
}

} // namespace framework::graphics
