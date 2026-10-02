#include "cel_quads.hpp"

namespace framework::graphics {
namespace {

float px(fix32 v) { return static_cast<float>(v.raw) / static_cast<float>(fix32::ONE); }

} // namespace

bool cel_quad(const ClipCel& cel, const CelPlace& at, TextureSize sheet, render::Quad& out) {
    out = render::Quad{};
    if (at.zoom < 1 || cel.w == 0 || cel.h == 0 || cel.x > sheet.w || cel.w > sheet.w - cel.x || cel.y > sheet.h ||
        cel.h > sheet.h - cel.y)
        return false;
    const ScreenRect r = cel_screen_rect(cel, at);
    out.x = static_cast<float>(r.x);
    out.y = static_cast<float>(r.y);
    out.w = static_cast<float>(r.w);
    out.h = static_cast<float>(r.h);
    out.tx = static_cast<float>(cel.x);
    out.ty = static_cast<float>(cel.y);
    out.tw = static_cast<float>(cel.w);
    out.th = static_cast<float>(cel.h);
    out.flip = at.flip_h ? SPRITE_FLIP_H : 0;
    return true;
}

DebugQuadStats debug_quads(std::span<const DebugQuad> in, std::span<render::Quad> out) {
    DebugQuadStats st;
    for (const DebugQuad& d : in) {
        if (st.quads == out.size()) {
            ++st.dropped;
            continue;
        }
        render::Quad& q = out[st.quads++];
        q = render::Quad{};
        if (d.dir.x.raw != fix32::ONE || d.dir.y.raw != 0) {
            ++st.rejected;
            continue;
        }
        q.x = px(d.center.x - d.half.x);
        q.y = px(d.center.y - d.half.y);
        q.w = px(d.half.x) * 2;
        q.h = px(d.half.y) * 2;
        q.tw = 1;
        q.th = 1;
        q.rgba = d.rgba;
    }
    return st;
}

} // namespace framework::graphics
