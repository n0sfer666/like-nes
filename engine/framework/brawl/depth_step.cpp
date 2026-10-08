#include "depth_step.hpp"

namespace framework::brawl {

namespace {

fix32 unit(fix32 v) {
    const fix32 one = fix32::from_int(1);
    if (one < v) return one;
    if (v < -one) return -one;
    return v;
}

void drive(DepthBody& d, int8_t& facing, const BrawlInput& in, const DepthProfile& p) {
    if (!in.present) {
        d.vx = fix32{};
        d.vz = fix32{};
        return;
    }
    const fix32 mx = unit(in.move.move_x);
    d.vx = mx * p.speed_x;
    d.vz = unit(in.move.move_z) * p.speed_z;
    if (mx < fix32{}) facing = -1;
    if (fix32{} < mx) facing = 1;
    if (grounded(d) && (in.buttons & button::JUMP) != 0) d.vy = p.jump_vy;
}

void fall(DepthBody& d, const DepthProfile& p) {
    if (!(fix32{} < d.y) && !(fix32{} < d.vy)) return;
    d.vy = d.vy - p.gravity;
    d.y = d.y + d.vy;
    if (fix32{} < d.y) return;
    d.y = fix32{};
    d.vy = fix32{};
}

} // namespace

bool grounded(const DepthBody& d) { return d.y == fix32{} && d.vy == fix32{}; }

void step_body(Body& b, const BrawlInput& in, const DepthProfile& p, const DepthFloor& f) {
    drive(b.pos, b.facing, in, p);
    coast_body(b, p, f);
}

void coast_body(Body& b, const DepthProfile& p, const DepthFloor& f) {
    fall(b.pos, p);
    slide(f, b.pos);
    ++b.age;
}

} // namespace framework::brawl
