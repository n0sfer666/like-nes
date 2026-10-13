#include "rope_swing.hpp"

#include "fixtrig.hpp"

namespace framework::brawl {

namespace {

constexpr fix32 TWO_PI = fix32::from_raw(411775);

} // namespace

fix32 rope_turns(fix32 arc, fix32 length) { return arc / (length * TWO_PI); }

fix32 rope_cap(fix32 length) { return length * TWO_PI / fix32::from_int(4); }

Vec2 rope_end(Vec2 pivot, fix32 length, fix32 arc) {
    const fix32 t = rope_turns(arc, length);
    return {pivot.x + length * sin_turns(t), pivot.y + length * cos_turns(t)};
}

Vec2 rope_tangent(fix32 arc, fix32 length) {
    const fix32 t = rope_turns(arc, length);
    return {cos_turns(t), -sin_turns(t)};
}

void swing_rope(RopeState& r, fix32 length, fix32 g, fix32 push) {
    r.speed = r.speed - g * sin_turns(rope_turns(r.arc, length)) + push;
    r.arc = r.arc + r.speed;
    r.speed = r.speed - fix32::from_raw(r.speed.raw / ROPE_DAMPING);
    const fix32 cap = rope_cap(length);
    if (!(cap < abs_fix(r.arc))) return;
    r.arc = clamp_fix(r.arc, -cap, cap);
    r.speed = fix32{};
}

} // namespace framework::brawl
