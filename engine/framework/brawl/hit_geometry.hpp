#pragma once
#include "brawl_body.hpp"
#include "clip_format.hpp"

namespace framework::brawl {

struct HitRect {
    fix32 x0, x1, h0, h1;
};

HitRect place_box(const Body& b, const graphics::Rect16& box);
bool rects_cross(const HitRect& a, const HitRect& b);
bool depths_cross(fix32 z_a, fix32 depth_a, fix32 z_t, fix32 depth_t);

} // namespace framework::brawl
