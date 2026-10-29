#include "pendulum.hpp"

#include "fixtrig.hpp"

namespace framework::physics {

namespace {

constexpr fix32 TWO_PI = fix32::from_raw(411775);

} // namespace

fix32 pendulum_turns(fix32 arc, fix32 length) { return arc / (length * TWO_PI); }

fix32 pendulum_cap(fix32 length) { return length * TWO_PI / fix32::from_int(4); }

Vec2 pendulum_end(Vec2 pivot, fix32 length, fix32 arc) {
    const fix32 t = pendulum_turns(arc, length);
    return {pivot.x + length * sin_turns(t), pivot.y + length * cos_turns(t)};
}

Vec2 pendulum_tangent(fix32 arc, fix32 length) {
    const fix32 t = pendulum_turns(arc, length);
    return {cos_turns(t), -sin_turns(t)};
}

void swing_pendulum(Pendulum& p, fix32 length, fix32 g, fix32 push, int32_t damping) {
    p.speed = p.speed - g * sin_turns(pendulum_turns(p.arc, length)) + push;
    p.arc = p.arc + p.speed;
    p.speed = p.speed - fix32::from_raw(p.speed.raw / damping);
    const fix32 cap = pendulum_cap(length);
    if (!(cap < abs_fix(p.arc))) return;
    p.arc = clamp_fix(p.arc, -cap, cap);
    p.speed = fix32{};
}

} // namespace framework::physics
