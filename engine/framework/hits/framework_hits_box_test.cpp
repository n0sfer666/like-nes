#include "hit_box.hpp"

#include <array>

#include "framework_hits_check.hpp"
#include "platform_args.hpp"

namespace {

using namespace framework::hits;
using framework::hits::test::check;

fix32 px(int32_t v) { return fix32::from_int(v); }

bool same(HitRect a, HitRect b) { return a.x0 == b.x0 && a.x1 == b.x1 && a.h0 == b.h0 && a.h1 == b.h1; }

using Placer = HitRect (*)(const framework::depth::DepthBody&, int8_t, const framework::graphics::Rect16&);

bool placement(Placer place) {
    const framework::depth::DepthBody body{px(100), px(99), px(40), px(7), px(-2), px(3)};
    const framework::graphics::Rect16 box{8, -24, 12, 6};
    return same(place(body, 1, box), HitRect{px(108), px(120), px(58), px(64)}) &&
           same(place(body, -1, box), HitRect{px(80), px(92), px(58), px(64)}) &&
           same(place(body, 0, box), place(body, 1, box)) &&
           same(place(body, -1, {-9, 5, 3, 4}), HitRect{px(106), px(109), px(31), px(35)});
}

HitRect no_width_in_mirror(const framework::depth::DepthBody& b, int8_t facing, const framework::graphics::Rect16& box) {
    HitRect out = place_box(b, facing, box);
    if (facing < 0) { out.x0 = out.x0 + px(box.w); out.x1 = out.x1 + px(box.w); }
    return out;
}

HitRect wrong_height(const framework::depth::DepthBody& b, int8_t facing, const framework::graphics::Rect16& box) {
    HitRect out = place_box(b, facing, box);
    out.h0 = out.h0 + px(box.y * 2);
    out.h1 = out.h1 + px(box.y * 2);
    return out;
}

using Crosser = bool (*)(const HitRect&, const HitRect&);

bool edges(Crosser cross) {
    const fix32 raw = fix32::from_raw(1);
    const HitRect a{px(10), px(20), px(30), px(40)};
    const std::array<HitRect, 4> touch{{
        {px(20), px(25), px(31), px(39)}, {px(5), px(10), px(31), px(39)},
        {px(11), px(19), px(40), px(45)}, {px(11), px(19), px(25), px(30)}}};
    for (const HitRect& b : touch)
        if (cross(a, b) || cross(b, a)) return false;
    const std::array<HitRect, 4> overlap{{
        {px(20) - raw, px(25), px(31), px(39)}, {px(5), px(10) + raw, px(31), px(39)},
        {px(11), px(19), px(40) - raw, px(45)}, {px(11), px(19), px(25), px(30) + raw}}};
    for (const HitRect& b : overlap)
        if (!cross(a, b) || !cross(b, a)) return false;
    return cross(a, a) && cross(a, {px(11), px(19), px(31), px(39)}) &&
           !cross(a, {px(21), px(25), px(41), px(45)});
}

bool closed_edges(const HitRect& a, const HitRect& b) {
    return !(b.x1 < a.x0 || a.x1 < b.x0 || b.h1 < a.h0 || a.h1 < b.h0);
}

bool no_height(const HitRect& a, const HitRect& b) { return a.x0 < b.x1 && b.x0 < a.x1; }

using DepthCross = bool (*)(fix32, fix32, fix32, fix32);

bool depth_edges(DepthCross cross) {
    const fix32 raw = fix32::from_raw(1);
    for (int sign : {-1, 1}) {
        const fix32 z = px(10), gap = px(sign * 7);
        if (cross(z, px(3), z + gap, px(4)) || cross(z + gap, px(4), z, px(3))) return false;
        if (!cross(z, px(3), z + gap - px(sign) * raw, px(4))) return false;
        if (cross(z, px(3), z + gap + px(sign) * raw, px(4))) return false;
    }
    return cross(px(5), px(2), px(5), px(1)) && !cross(px(5), fix32{}, px(5), fix32{});
}

bool signed_gap(fix32 a, fix32 da, fix32 b, fix32 db) { return b - a < da + db; }
bool closed_depth(fix32 a, fix32 da, fix32 b, fix32 db) {
    const fix32 gap = a < b ? b - a : a - b;
    return !(da + db < gap);
}

} // namespace

int main(int argc, char** argv) {
    const platform::Args args(argc, argv);
    check(placement(place_box), "box placement mirrors width and converts screen y into height");
    check(!placement(no_width_in_mirror) && !placement(wrong_height), "control: bad mirror and height are detected");
    check(edges(rects_cross), "all four open edges reject touching and accept one raw of overlap");
    check(!edges(closed_edges) && !edges(no_height), "control: closed edges and a lost axis are detected");
    check(depth_edges(depths_cross), "depth is symmetric and uses an open sum of half depths");
    check(!depth_edges(signed_gap) && !depth_edges(closed_depth), "control: signed gap and closed depth are detected");
    return framework::hits::test::verdict("framework-hits-box");
}
