#include "stage_leash.hpp"

#include "platform_body.hpp"
#include "slide.hpp"

namespace framework::brawl {

namespace {

fix32 zone(fix32 v, fix32 center, fix32 half) { return clamp_fix(v, center - half, center + half); }

void belt_leash(Body& b, const Leash& l, const DepthFloor* f) {
    const fix32 x = zone(b.pos.x, l.center.x, l.half.x);
    if (!(b.pos.x == x)) {
        b.pos.vx = fix32{};
        b.pos.x = f != nullptr ? move_x(*f, b.pos.x, b.pos.z, x) : x;
        if (l.crush_x && !(b.pos.x == x)) b.crushed = true;
    }
    if (!l.push_y) return;
    const fix32 feet = b.pos.z - b.pos.y;
    const fix32 to = zone(feet, l.center.y, l.half.y);
    if (feet == to) return;
    const fix32 z = b.pos.z + (to - feet);
    b.pos.vz = fix32{};
    b.pos.z = f != nullptr ? move_z(*f, b.pos.x, b.pos.z, z) : z;
    if (!(b.pos.z == z)) b.crushed = true;
}

bool shove(character::Character& c, Vec2 travel, const PlatformWorld& p) {
    const fix32 to_y = c.position.y + travel.y;
    const character::SlideResult r =
        character::move_and_slide(p.scene, p.hull, travel, p.profile.max_slope, c.position, c.velocity);
    if (!(travel.x == fix32{})) return r.hit_wall;
    if (travel.y < fix32{}) return r.hit_ceiling;
    return !(c.position.y == to_y);
}

void platform_leash(Body& b, character::Character& c, const Leash& l, const PlatformWorld& p) {
    const fix32 foot = hull_foot(p);
    bool moved = false;
    bool crushed = false;
    const fix32 x = zone(c.position.x, l.center.x, l.half.x);
    if (!(c.position.x == x)) {
        moved = true;
        crushed = shove(c, {x - c.position.x, fix32{}}, p) && l.crush_x;
        c.velocity.x = fix32{};
    }
    const fix32 feet = c.position.y + foot;
    const fix32 to = zone(feet, l.center.y, l.half.y);
    if (l.push_y && !(feet == to)) {
        moved = true;
        crushed = shove(c, {fix32{}, to - feet}, p) || crushed;
        c.velocity.y = fix32{};
    }
    if (moved) mirror_body(c, foot, p.dt, b.pos);
    if (crushed) b.crushed = true;
}

} // namespace

void leash_body(Body& b, SectionBody* s, const Leash& l, const BrawlWorld& w) {
    if (s != nullptr && w.platform != nullptr) platform_leash(b, s->c, l, *w.platform);
    else belt_leash(b, l, w.floor);
}

} // namespace framework::brawl
