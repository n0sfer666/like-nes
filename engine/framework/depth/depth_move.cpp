#include "depth_move.hpp"

namespace framework::depth {

namespace {

fix32 unit(fix32 v) {
    const fix32 one = fix32::from_int(1);
    if (one < v) return one;
    if (v < -one) return -one;
    return v;
}

} // namespace

bool grounded(const DepthBody& d) { return d.y == fix32{} && d.vy == fix32{}; }

void drive(DepthBody& d, const DepthIntent& in, const DepthProfile& p) {
    d.vx = unit(in.mx) * p.speed_x;
    d.vz = unit(in.mz) * p.speed_z;
    if (grounded(d) && in.hop) d.vy = p.jump_vy;
}

void fall(DepthBody& d, const DepthProfile& p) {
    if (!(fix32{} < d.y) && !(fix32{} < d.vy)) return;
    d.vy = d.vy - p.gravity;
    d.y = d.y + d.vy;
    if (fix32{} < d.y) return;
    d.y = fix32{};
    d.vy = fix32{};
}

} // namespace framework::depth
