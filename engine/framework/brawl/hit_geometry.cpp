#include "hit_geometry.hpp"

namespace framework::brawl {

HitRect place_box(const Body& b, const graphics::Rect16& box) {
    const int32_t left = b.facing < 0 ? -(int32_t{box.x} + box.w) : int32_t{box.x};
    const fix32 x0 = b.pos.x + fix32::from_int(left);
    const fix32 top = b.pos.y - fix32::from_int(box.y);
    return HitRect{x0, x0 + fix32::from_int(box.w), top - fix32::from_int(box.h), top};
}

bool rects_cross(const HitRect& a, const HitRect& b) {
    return a.x0 < b.x1 && b.x0 < a.x1 && a.h0 < b.h1 && b.h0 < a.h1;
}

bool depths_cross(fix32 z_a, fix32 depth_a, fix32 z_t, fix32 depth_t) {
    const fix32 gap = z_a < z_t ? z_t - z_a : z_a - z_t;
    return gap < depth_a + depth_t;
}

} // namespace framework::brawl
