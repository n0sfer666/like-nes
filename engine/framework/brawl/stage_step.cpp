#include "stage_step.hpp"

#include <array>

#include "autoscroll_step.hpp"
#include "level_clamp.hpp"
#include "platform_world.hpp"
#include "scene_objects.hpp"
#include "section_mode.hpp"
#include "section_step.hpp"
#include "stage_leash.hpp"
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

fix32 screen_y(const Body& b) { return b.pos.z - b.pos.y; }

fix32 middle(const Live& live, fix32 (*of)(const Body&)) {
    fix32 lo = of(*live.at[0]);
    fix32 hi = lo;
    for (uint32_t i = 1; i < live.count; ++i) {
        lo = min_fix(lo, of(*live.at[i]));
        hi = max_fix(hi, of(*live.at[i]));
    }
    return mid_fix(lo, hi);
}

fix32 dead_zone(fix32 center, fix32 target, fix32 dead) { return clamp_fix(center, target - dead, target + dead); }

bool follows_y(const BrawlWorld& w, fix32 x) {
    return w.platform != nullptr && w.level != nullptr && mode_at(*w.level, x) == scene::SectionMode::Platform;
}

} // namespace

void step_camera(StageCamera& camera, BodyPool& pool, const Seats& seats, StageSections& sections,
                 const BrawlWorld& w) {
    const Live live = live_players(pool, seats);
    if (live.count == 0) return;
    const Vec2 before = camera.center;
    camera.center.x = middle(live, [](const Body& b) { return b.pos.x; });
    if (follows_y(w, camera.center.x))
        camera.center.y = dead_zone(camera.center.y, middle(live, screen_y), w.platform->dead_y);
    const Vec2 half = graphics::view_zone_half();
    Ride ride;
    if (w.level != nullptr) {
        ride = ride_scroll(camera, before, *w.level);
        camera.center = clamp_level(camera.center, camera.arena, *w.level, half);
    }
    const Leash leash{camera.center, half, ride.on && ride.axis == scene::ScrollAxis::X,
                      ride.on && ride.axis == scene::ScrollAxis::Y};
    for (uint32_t i = 0; i < live.count; ++i) {
        Body& b = *live.at[i];
        leash_body(b, sections.find(b.id), leash, w);
        const fix32 x = b.pos.x;
        cross_section(sections, pool, b, w);
        if (leash.crush_x && !(b.pos.x == x)) b.crushed = true;
    }
}

void step_stage(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, StageCamera& camera,
                const Seats& seats, StageSections& sections, HitEvents& events) {
    enter_spawned(sections, pool, w);
    step_bodies(pool, commands, w, &sections);
    step_sections(sections, pool, w);
    step_camera(camera, pool, seats, sections, w);
    resolve_hits(pool, w, &sections, events);
}

} // namespace framework::brawl
