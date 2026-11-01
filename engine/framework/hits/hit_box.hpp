#pragma once
#include "clip_format.hpp"
#include "depth_body.hpp"

namespace framework::hits {

struct HitRect {
    fix32 x0, x1, h0, h1;
};

HitRect place_box(const depth::DepthBody& b, int8_t facing, const graphics::Rect16& box);
bool rects_cross(const HitRect& a, const HitRect& b);
bool depths_cross(fix32 z_a, fix32 depth_a, fix32 z_t, fix32 depth_t);

} // namespace framework::hits
