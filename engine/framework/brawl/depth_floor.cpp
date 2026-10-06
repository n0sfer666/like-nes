#include "depth_floor.hpp"

namespace framework::brawl {

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

fix32 move_x(const DepthFloor& f, fix32 x, fix32 z, fix32 to) {
    for (uint32_t i = 0; i < f.wall_count; ++i) {
        const FloorRect& w = f.walls[i];
        if (inside(z, w.z0, w.z1)) to = sweep(x, to, w.x0, w.x1);
    }
    return clamp(to, f.band.x0, f.band.x1);
}

fix32 move_z(const DepthFloor& f, fix32 x, fix32 z, fix32 to) {
    for (uint32_t i = 0; i < f.wall_count; ++i) {
        const FloorRect& w = f.walls[i];
        if (inside(x, w.x0, w.x1)) to = sweep(z, to, w.z0, w.z1);
    }
    return clamp(to, f.band.z0, f.band.z1);
}

} // namespace

bool DepthFloor::add_wall(const FloorRect& r) {
    if (wall_count == MAX_WALLS || r.x1 < r.x0 || r.z1 < r.z0) return false;
    walls[wall_count++] = r;
    return true;
}

void slide(const DepthFloor& f, DepthBody& b) {
    const fix32 want_x = b.x + b.vx;
    b.x = move_x(f, b.x, b.z, want_x);
    if (!(b.x == want_x)) b.vx = fix32{};
    const fix32 want_z = b.z + b.vz;
    b.z = move_z(f, b.x, b.z, want_z);
    if (!(b.z == want_z)) b.vz = fix32{};
}

} // namespace framework::brawl
