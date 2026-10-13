#include "rope_ride.hpp"

#include "body_clip.hpp"
#include "collision.hpp"
#include "platform_body.hpp"

namespace framework::brawl {

namespace {

uint32_t slot(const BodyPool& pool, EntId id) {
    uint32_t i = 0;
    while (i < pool.count && !(pool.bodies[i].id == id)) ++i;
    return i;
}

bool holds(const Body& b) { return b.hp > 0 && b.react == Reaction::None && b.grip.by == EntId{}; }

bool pressed(const BrawlInput& in, uint16_t mask) { return in.present && (in.buttons & mask) != 0; }

fix32 gravity(const PlatformWorld& p) { return p.profile.gravity_fall * p.dt * p.dt; }

Vec2 hang_point(const scene::Rope& rope, fix32 arc, const PlatformWorld& p) {
    const Vec2 end = rope_end(rope.mark.at, rope.length, arc);
    return {end.x, end.y + hull_half(p).y};
}

void hang(character::Character& c, const RopeState& r, fix32 length, fix32 dt) {
    c.velocity = rope_tangent(r.arc, length) * (r.speed / dt);
    c.state = character::MoveState::Air;
    c.coyote_left = 0;
    c.buffer_left = 0;
    c.jump_active = false;
    c.support = physics::BodyId{physics::BodyId::INVALID};
}

bool blocked(const PlatformWorld& p, Vec2 from, Vec2 to) {
    character::SceneHit hit;
    return character::cast_nearest(p.scene, p.hull, from, to - from, hit);
}

bool within_reach(Vec2 hands, Vec2 end) {
    const int64_t dx = (hands.x - end.x).raw;
    const int64_t dy = (hands.y - end.y).raw;
    const uint64_t reach = static_cast<uint64_t>(ROPE_REACH.raw);
    return static_cast<uint64_t>(dx * dx) + static_cast<uint64_t>(dy * dy) <= reach * reach;
}

EntId let_go(RopeState& r) {
    const EntId gone = r.rider;
    r.rider = EntId{};
    r.cooldown = ROPE_COOLDOWN;
    return gone;
}

EntId fall_off(RopeState& r, const scene::Rope& rope, const PlatformWorld& p) {
    const EntId gone = r.rider == EntId{} ? EntId{} : let_go(r);
    swing_rope(r, rope.length, gravity(p), fix32{});
    return gone;
}

fix32 pump(const BrawlInput& in, Body& b) {
    if (!in.present) return {};
    const fix32 x = clamp_fix(in.move.move_x, -fix32::from_int(1), fix32::from_int(1));
    if (x < fix32{}) b.facing = -1;
    if (fix32{} < x) b.facing = 1;
    return x * ROPE_PUMP;
}

} // namespace

EntId ride_rope(RopeState& r, const scene::Rope& rope, BodyPool& pool, std::span<const Command> commands,
                StageSections& sections, const BrawlWorld& w) {
    const PlatformWorld& p = *w.platform;
    if (r.cooldown > 0) --r.cooldown;
    const uint32_t i = r.rider == EntId{} ? pool.count : slot(pool, r.rider);
    Body* b = i < pool.count ? &pool.bodies[i] : nullptr;
    SectionBody* s = b != nullptr ? sections.find(b->id) : nullptr;
    if (s == nullptr || !holds(*b)) return fall_off(r, rope, p);
    if (b->hitstop > 0) return {};
    const fix32 foot = hull_foot(p);
    adopt_body(b->pos, foot, p.dt, s->c);
    const Vec2 hung = hang_point(rope, r.arc, p);
    if (!(s->c.position.x == hung.x && s->c.position.y == hung.y)) return fall_off(r, rope, p);
    const bool strikes = b->kind < w.kinds.size() && striking(*b, w.kinds[b->kind]);
    const BrawlInput in = !strikes && i < commands.size() ? commands[i].input : BrawlInput{};
    const bool jump = pressed(in, button::JUMP);
    if (jump && !s->c.jump_was_held) {
        hang(s->c, r, rope.length, p.dt);
        s->c.jump_was_held = true;
        mirror_body(s->c, foot, p.dt, b->pos);
        return fall_off(r, rope, p);
    }
    s->c.jump_was_held = jump;
    const fix32 arc = r.arc;
    swing_rope(r, rope.length, gravity(p), pump(in, *b));
    const Vec2 to = hang_point(rope, r.arc, p);
    character::SceneHit hit;
    if (character::cast_nearest(p.scene, p.hull, s->c.position, to - s->c.position, hit)) {
        if (hit.body.valid()) return let_go(r);
        r.arc = arc;
        r.speed = fix32{};
    } else {
        s->c.position = to;
    }
    hang(s->c, r, rope.length, p.dt);
    mirror_body(s->c, foot, p.dt, b->pos);
    return {};
}

void grab_rope(StageRopes& ropes, const scene::LevelObjects& level, Body& b, const BrawlInput& in,
               StageSections& sections, const PlatformWorld& p) {
    if (!pressed(in, button::GRAB) || !holds(b) || b.hitstop > 0 || rides(&ropes, b.id)) return;
    SectionBody* s = sections.find(b.id);
    if (s == nullptr || s->c.on_ground) return;
    const fix32 foot = hull_foot(p);
    adopt_body(b.pos, foot, p.dt, s->c);
    const Vec2 hands{s->c.position.x, s->c.position.y - hull_half(p).y};
    for (size_t i = 0; i < ropes.at.size(); ++i) {
        RopeState& r = ropes.at[i];
        const scene::Rope& rope = level.ropes[i];
        if (!(r.rider == EntId{}) || r.cooldown > 0) continue;
        if (!within_reach(hands, rope_end(rope.mark.at, rope.length, r.arc))) continue;
        const Vec2 to = hang_point(rope, r.arc, p);
        if (blocked(p, s->c.position, to)) continue;
        r.rider = b.id;
        r.speed = dot(s->c.velocity * p.dt, rope_tangent(r.arc, rope.length));
        s->c.position = to;
        s->c.jump_was_held = pressed(in, button::JUMP);
        hang(s->c, r, rope.length, p.dt);
        mirror_body(s->c, foot, p.dt, b.pos);
        return;
    }
}

} // namespace framework::brawl
