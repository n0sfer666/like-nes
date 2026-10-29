#include "depth_floor.hpp"

namespace framework::depth {

namespace {

bool inside(fix32 v, fix32 lo, fix32 hi) { return !(v < lo) && !(hi < v); }

fix32 clamp(fix32 v, fix32 lo, fix32 hi) {
    if (v < lo) return lo;
    if (hi < v) return hi;
    return v;
}

fix32 sweep(fix32 from, fix32 to, fix32 lo, fix32 hi) {
    if (from < to && !(lo < from) && lo < to) return lo;
    if (to < from && !(from < hi) && to < hi) return hi;
    return to;
}

} // namespace

fix32 move_z(FloorView f, fix32 x, fix32 z, fix32 to) {
    for (const FloorRect& w : f.walls) {
        if (inside(x, w.x0, w.x1)) to = sweep(z, to, w.z0, w.z1);
    }
    return clamp(to, f.band.z0, f.band.z1);
}

bool blocked(FloorView f, fix32 x, fix32 z) {
    if (!inside(x, f.band.x0, f.band.x1) || !inside(z, f.band.z0, f.band.z1)) return true;
    for (const FloorRect& w : f.walls) {
        if (inside(x, w.x0, w.x1) && inside(z, w.z0, w.z1)) return true;
    }
    return false;
}

fix32 move_x(FloorView f, fix32 x, fix32 z, fix32 to) {
    for (const FloorRect& w : f.walls) {
        if (inside(z, w.z0, w.z1)) to = sweep(x, to, w.x0, w.x1);
    }
    return clamp(to, f.band.x0, f.band.x1);
}

void slide(FloorView f, DepthBody& b) {
    const fix32 want_x = b.x + b.vx;
    b.x = move_x(f, b.x, b.z, want_x);
    if (!(b.x == want_x)) b.vx = fix32{};
    const fix32 want_z = b.z + b.vz;
    b.z = move_z(f, b.x, b.z, want_z);
    if (!(b.z == want_z)) b.vz = fix32{};
}

} // namespace framework::depth
