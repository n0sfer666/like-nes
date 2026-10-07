#include "rumble_depth_overlay.hpp"

namespace rumble {

namespace {

using namespace framework::graphics;

fix32 whole(int32_t v) { return fix32::from_int(v); }

int32_t pixels(fix32 world, int32_t zoom) { return (world * whole(zoom)).to_int(); }

void band(DebugDraw& dd, const ScreenRect& box, const CelPlace& at, fix32 lift, fix32 depth, uint32_t rgba) {
    if (!(fix32{} < depth) || box.w <= 0) return;
    const int32_t top = at.y + pixels(lift - depth, at.zoom);
    const int32_t bottom = at.y + pixels(lift + depth, at.zoom);
    const Vec2 half{whole(box.w) / whole(2), whole(bottom - top) / whole(2)};
    dd.frame({whole(box.x) + half.x, whole(top) + half.y}, half, whole(1), rgba);
}

} // namespace

void draw_depth(DebugDraw& dd, const Pose& pose, const CelPlace& at, const DepthOverlay& depth) {
    for (const ClipBox& b : frame_boxes(pose.clip, pose.frame, BoxKind::Hurt))
        band(dd, box_screen_rect(b.rect, at), at, depth.lift, depth.body, box_debug_rgba(BoxKind::Hurt));
    for (const ClipBox& b : frame_boxes(pose.clip, pose.frame, BoxKind::Hit))
        if (b.index < depth.hit.size())
            band(dd, box_screen_rect(b.rect, at), at, depth.lift, depth.hit[b.index], box_debug_rgba(BoxKind::Hit));
}

} // namespace rumble
