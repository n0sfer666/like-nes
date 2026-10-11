#include "stage_step.hpp"

#include <array>

#include "camera.hpp"
#include "scene_objects.hpp"
#include "viewport_fit.hpp"

namespace framework::brawl {

namespace {

struct Live {
    std::array<Body*, SEATS> at{};
    uint32_t count = 0;
};

Live live_players(BodyPool& pool, const Seats& seats) {
    Live live;
    for (const Seat& s : seats.at) {
        if (!s.present) continue;
        Body* b = pool.find(s.body);
        if (b != nullptr && b->hp > 0) live.at[live.count++] = b;
    }
    return live;
}

Vec2 clamp_into(Vec2 c, const scene::LevelRect& r, Vec2 half) {
    return {graphics::clamp_axis(c.x, r.x0, r.x1, half.x), graphics::clamp_axis(c.y, r.y0, r.y1, half.y)};
}

Vec2 clamp_level(Vec2 c, int16_t arena, const scene::LevelObjects& level, Vec2 half) {
    for (const scene::Arena& a : level.arenas)
        if (arena != NO_ARENA && a.id == arena) c = clamp_into(c, a.area.rect, half);
    if (level.bounds) c = clamp_into(c, level.bounds->rect, half);
    return c;
}

void leash(Body& b, fix32 center, fix32 half, const DepthFloor* floor) {
    const fix32 x = clamp_fix(b.pos.x, center - half, center + half);
    if (b.pos.x == x) return;
    b.pos.vx = fix32{};
    b.pos.x = floor != nullptr ? move_x(*floor, b.pos.x, b.pos.z, x) : x;
}

} // namespace

void step_camera(StageCamera& camera, BodyPool& pool, const Seats& seats, const BrawlWorld& w) {
    const Live live = live_players(pool, seats);
    if (live.count == 0) return;
    fix32 lo = live.at[0]->pos.x;
    fix32 hi = lo;
    for (uint32_t i = 1; i < live.count; ++i) {
        lo = min_fix(lo, live.at[i]->pos.x);
        hi = max_fix(hi, live.at[i]->pos.x);
    }
    camera.center.x = mid_fix(lo, hi);
    const Vec2 half = graphics::view_zone_half();
    if (w.level != nullptr) camera.center = clamp_level(camera.center, camera.arena, *w.level, half);
    for (uint32_t i = 0; i < live.count; ++i) leash(*live.at[i], camera.center.x, half.x, w.floor);
}

void step_stage(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, StageCamera& camera,
                const Seats& seats, HitEvents& events) {
    step_bodies(pool, commands, w);
    step_camera(camera, pool, seats, w);
    resolve_hits(pool, w, events);
}

} // namespace framework::brawl
