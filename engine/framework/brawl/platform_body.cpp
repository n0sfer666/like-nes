#include "platform_body.hpp"

#include "controller.hpp"

namespace framework::brawl {

namespace {

fix32 extent(const physics::Shape& s, bool vertical) {
    fix32 far{};
    for (uint32_t i = 0; i < s.count; ++i) far = max_fix(far, abs_fix(vertical ? s.points[i].y : s.points[i].x));
    return far + s.radius;
}

fix32 air_vy(const character::Character& c, fix32 dt) { return c.on_ground ? fix32{} : -(c.velocity.y * dt); }

character::MoveInput drive(Body& b, const BrawlInput& in) {
    character::MoveInput m;
    if (!in.present) return m;
    m.move_x = clamp_fix(in.move.move_x, -fix32::from_int(1), fix32::from_int(1));
    m.jump_held = (in.buttons & button::JUMP) != 0;
    m.down_held = in.move.down_held;
    m.up_held = in.move.up_held;
    if (m.move_x < fix32{}) b.facing = -1;
    if (fix32{} < m.move_x) b.facing = 1;
    return m;
}

} // namespace

fix32 hull_foot(const PlatformWorld& p) { return extent(p.hull.shape, true) + character::SKIN; }

Vec2 hull_half(const PlatformWorld& p) { return {extent(p.hull.shape, false), extent(p.hull.shape, true)}; }

void mirror_body(const character::Character& c, fix32 foot, fix32 dt, DepthBody& d) {
    const fix32 feet = c.position.y + foot;
    d.x = c.position.x;
    if (c.on_ground || d.z < feet) d.z = feet;
    d.y = d.z - feet;
    d.vx = c.velocity.x * dt;
    d.vz = fix32{};
    d.vy = air_vy(c, dt);
}

void adopt_body(const DepthBody& d, fix32 foot, fix32 dt, character::Character& c) {
    c.position = {d.x, d.z - d.y - foot};
    if (!(d.vx == c.velocity.x * dt)) c.velocity.x = d.vx / dt;
    if (d.vy == air_vy(c, dt)) return;
    c.velocity.y = -(d.vy / dt);
    if (!(fix32{} < d.vy)) return;
    c.state = character::MoveState::Air;
    c.on_ground = false;
    c.coyote_left = 0;
}

void step_platform(Body& b, const BrawlInput& in, bool held, SectionBody& s, const PlatformWorld& p) {
    const fix32 foot = hull_foot(p);
    adopt_body(b.pos, foot, p.dt, s.c);
    const character::MoveInput m = held ? character::MoveInput{} : drive(b, in);
    character::step(p.scene, p.hull, p.profile, p.derived, m, p.dt, s.c);
    mirror_body(s.c, foot, p.dt, b.pos);
    b.crushed = s.c.crushed;
    ++b.age;
}

} // namespace framework::brawl
