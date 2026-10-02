#include "clip_debug.hpp"

namespace framework::graphics {
namespace {

constexpr BoxKind KINDS[3] = {BoxKind::Hit, BoxKind::Hurt, BoxKind::Push};

fix32 whole(int32_t v) { return fix32::from_int(v); }

Vec2 half_of(const ScreenRect& r) { return {whole(r.w) / whole(2), whole(r.h) / whole(2)}; }

Vec2 center_of(const ScreenRect& r) {
    const Vec2 h = half_of(r);
    return {whole(r.x) + h.x, whole(r.y) + h.y};
}

void outline(DebugDraw& dd, const ScreenRect& r, uint32_t rgba) {
    dd.frame(center_of(r), half_of(r), whole(1), rgba);
}

void fill(DebugDraw& dd, const ScreenRect& r, uint32_t rgba) { dd.fill(center_of(r), half_of(r), rgba); }

void pivot_cross(DebugDraw& dd, const CelPlace& at) {
    fill(dd, {at.x - 2, at.y, 5, 1}, PIVOT_DEBUG_RGBA);
    fill(dd, {at.x, at.y - 2, 1, 2}, PIVOT_DEBUG_RGBA);
    fill(dd, {at.x, at.y + 1, 1, 2}, PIVOT_DEBUG_RGBA);
}

} // namespace

ScreenRect cel_screen_rect(const ClipCel& cel, const CelPlace& at) {
    const int32_t left = at.flip_h ? int32_t{cel.w} - cel.anchor_x : int32_t{cel.anchor_x};
    return {at.x - left * at.zoom, at.y - int32_t{cel.anchor_y} * at.zoom, int32_t{cel.w} * at.zoom,
            int32_t{cel.h} * at.zoom};
}

ScreenRect box_screen_rect(const Rect16& box, const CelPlace& at) {
    const int32_t left = at.flip_h ? -(int32_t{box.x} + box.w) : int32_t{box.x};
    return {at.x + left * at.zoom, at.y + int32_t{box.y} * at.zoom, int32_t{box.w} * at.zoom,
            int32_t{box.h} * at.zoom};
}

uint32_t box_debug_rgba(BoxKind kind) {
    switch (kind) {
    case BoxKind::Hit: return 0xff0000ffu;
    case BoxKind::Hurt: return 0x00ff00ffu;
    case BoxKind::Push: return 0xffff00ffu;
    }
    return CEL_DEBUG_RGBA;
}

void draw_cel_debug(DebugDraw& dd, const ClipView& view, uint16_t frame, const CelPlace& at) {
    const ClipCel* cel = frame_cel(view, frame);
    if (cel == nullptr) return;
    outline(dd, cel_screen_rect(*cel, at), CEL_DEBUG_RGBA);
    for (const BoxKind kind : KINDS)
        for (const ClipBox& b : frame_boxes(view, frame, kind))
            outline(dd, box_screen_rect(b.rect, at), box_debug_rgba(kind));
    pivot_cross(dd, at);
}

} // namespace framework::graphics
