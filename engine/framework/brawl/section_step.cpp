#include "section_step.hpp"

#include "body_kind.hpp"
#include "depth_step.hpp"
#include "platform_body.hpp"
#include "section_exit.hpp"
#include "section_mode.hpp"
#include "slide.hpp"

namespace framework::brawl {

namespace {

using scene::SectionMode;

character::Character entering(const Body& b, fix32 foot, fix32 dt) {
    character::Character c;
    c.position = {b.pos.x, b.pos.z - b.pos.y - foot};
    c.velocity = {b.pos.vx / dt, -(b.pos.vy / dt)};
    c.on_ground = grounded(b.pos);
    c.state = c.on_ground ? character::MoveState::Ground : character::MoveState::Air;
    return c;
}

void hold_belt(Body& b, const BrawlWorld& w) {
    b.pos.x = move_x(*w.floor, b.pos.x, b.pos.z, refusal_x(*w.level, b.pos.x, SectionMode::Belt));
    b.pos.vx = fix32{};
}

void hold_platform(Body& b, SectionBody& s, const BrawlWorld& w) {
    const PlatformWorld& p = *w.platform;
    const Vec2 travel{refusal_x(*w.level, b.pos.x, SectionMode::Platform) - s.c.position.x, fix32{}};
    character::move_and_slide(p.scene, p.hull, travel, p.profile.max_slope, s.c.position, s.c.velocity);
    s.c.velocity.x = fix32{};
    mirror_body(s.c, hull_foot(p), p.dt, b.pos);
}

bool free_at(const BodyPool& pool, const StageSections& sections, EntId self, const scene::LevelRect& r, fix32 x,
             character::Character& c, const BrawlWorld& w) {
    if (x < r.x0 || !(x < r.x1) || platform_taken(pool, sections, self, Vec2{x, c.position.y}, w)) return false;
    c.position.x = x;
    return true;
}

bool settle(const BodyPool& pool, const StageSections& sections, EntId self, character::Character& c,
            const BrawlWorld& w) {
    const scene::Section* here = section_at(*w.level, c.position.x);
    const fix32 half = hull_half(*w.platform).x;
    const fix32 step = half + half;
    if (here == nullptr || !(fix32{} < step)) return false;
    const scene::LevelRect& r = here->area.rect;
    const fix32 from = c.position.x;
    for (fix32 d = step; d < r.x1 - r.x0; d = d + step)
        if (free_at(pool, sections, self, r, from + d, c, w) || free_at(pool, sections, self, r, from - d, c, w))
            return true;
    return false;
}

void enter(Body& b, const BodyPool& pool, StageSections& sections, const BrawlWorld& w, bool spawned) {
    if (!spawned && !grounded(b.pos)) return;
    const PlatformWorld& p = *w.platform;
    const fix32 foot = hull_foot(p);
    character::Character c = entering(b, foot, p.dt);
    const bool taken = platform_taken(pool, sections, b.id, c.position, w);
    if ((taken && !(spawned && settle(pool, sections, b.id, c, w))) || !sections.add(b.id, c)) {
        hold_belt(b, w);
        return;
    }
    mirror_body(c, foot, p.dt, b.pos);
}

void leave(Body& b, const BodyPool& pool, StageSections& sections, SectionBody& s, const BrawlWorld& w) {
    if (!s.c.on_ground) return;
    const PlatformWorld& p = *w.platform;
    const fix32 z = clamp_fix(s.c.position.y + hull_foot(p), w.floor->band.z0, w.floor->band.z1);
    if (belt_taken(pool, sections, b, z, w)) {
        hold_platform(b, s, w);
        return;
    }
    b.pos = DepthBody{s.c.position.x, z, fix32{}, s.c.velocity.x * p.dt, fix32{}, fix32{}};
    sections.drop(b.id);
}

void cross(StageSections& sections, const BodyPool& pool, Body& b, const BrawlWorld& w, bool spawned) {
    if (w.platform == nullptr || w.level == nullptr || w.floor == nullptr) return;
    if (role_of(b, w.kinds) == Role::Projectile) return;
    SectionBody* s = sections.find(b.id);
    if (s != nullptr) adopt_body(b.pos, hull_foot(*w.platform), w.platform->dt, s->c);
    const bool platform = mode_at(*w.level, b.pos.x) == SectionMode::Platform;
    if (s == nullptr && platform) enter(b, pool, sections, w, spawned);
    if (s != nullptr && !platform) leave(b, pool, sections, *s, w);
}

} // namespace

void cross_section(StageSections& sections, const BodyPool& pool, Body& b, const BrawlWorld& w) {
    cross(sections, pool, b, w, false);
}

void step_sections(StageSections& sections, BodyPool& pool, const BrawlWorld& w) {
    sections.prune(pool);
    for (uint32_t i = 0; i < pool.count; ++i) cross_section(sections, pool, pool.bodies[i], w);
}

void enter_spawned(StageSections& sections, BodyPool& pool, const BrawlWorld& w) {
    sections.prune(pool);
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.bodies[i].age == 0) cross(sections, pool, pool.bodies[i], w, true);
}

} // namespace framework::brawl
