#pragma once
#include <cstdint>

#include "body_hash.hpp"
#include "body_pool.hpp"
#include "depth_step.hpp"
#include "hash_mix.hpp"

namespace framework::brawl::scenario {

constexpr fix32 px(int32_t v) { return fix32::from_int(v); }
constexpr fix32 part(int32_t num, int32_t den) { return fix32::from_raw(fix32::ONE * num / den); }

const DepthProfile WALKER{px(2), px(1), part(1, 2), px(6)};
const DepthProfile HEAVY{part(3, 2), part(3, 4), part(3, 4), px(7)};

inline DepthFloor stage() {
    DepthFloor f;
    f.band = FloorRect{px(0), px(192), px(640), px(224)};
    f.add_wall(FloorRect{px(200), px(200), px(240), px(224)});
    f.add_wall(FloorRect{px(400), px(192), px(410), px(208)});
    return f;
}

inline BrawlInput stick(fix32 mx, fix32 mz, uint16_t buttons = 0) {
    BrawlInput in;
    in.move.move_x = mx;
    in.move.move_z = mz;
    in.buttons = buttons;
    in.present = true;
    return in;
}

inline Body at(int32_t x, int32_t z) {
    Body b;
    b.pos.x = px(x);
    b.pos.z = px(z);
    b.hp = 100;
    return b;
}

using Script = BrawlInput (*)(uint32_t tick, uint32_t body);
using Event = void (*)(uint32_t tick, BodyPool& pool);

struct Scenario {
    const char* name;
    const DepthProfile* profile;
    uint32_t ticks;
    Script script;
    Event event;
};

inline void no_event(uint32_t, BodyPool&) {}

inline uint64_t run(const Scenario& s) {
    const DepthFloor floor = stage();
    BodyPool pool;
    pool.spawn(at(16, 208));
    pool.spawn(at(180, 196));
    uint64_t h = physics::FNV_OFFSET;
    for (uint32_t t = 0; t < s.ticks; ++t) {
        s.event(t, pool);
        for (uint32_t i = 0; i < pool.count; ++i)
            step_body(pool.bodies[i], s.script(t, pool.bodies[i].id.seq), *s.profile, floor);
        physics::mix_u64(h, state_hash(pool));
    }
    return h;
}

inline BrawlInput walk(uint32_t t, uint32_t) {
    if (t < 60) return stick(px(1), fix32{});
    if (t < 120) return stick(px(1), px(-1));
    return stick(px(-1), part(1, 2));
}

inline BrawlInput corner(uint32_t t, uint32_t body) {
    if (body == 1) return stick(fix32{}, fix32{});
    return t < 60 ? stick(px(1), px(1)) : stick(px(1), px(-1));
}

inline BrawlInput hop(uint32_t t, uint32_t) {
    const fix32 mx = (t / 40) % 2 == 0 ? px(1) : px(-1);
    return stick(mx, part(-1, 3), button::JUMP);
}

inline BrawlInput overdrive(uint32_t t, uint32_t body) {
    if (body == 2) return stick(px(3), px(-2));
    const uint16_t jump = t % 11 == 0 ? button::JUMP : uint16_t{0};
    return stick(part(-5, 2), part(static_cast<int32_t>(t % 9), 4), jump);
}

inline BrawlInput join_late(uint32_t t, uint32_t body) {
    BrawlInput in = stick(px(1), part(1, 3));
    if (body == 1 && t < 30) in.present = false;
    return in;
}

inline void leave_and_spawn(uint32_t t, BodyPool& pool) {
    if (t == 50) pool.despawn(EntId{2});
    if (t == 60) pool.spawn(at(300, 220));
}

inline BrawlInput to_the_ends(uint32_t, uint32_t body) {
    if (body == 1) return stick(px(-1), part(1, 4));
    if (body == 3) return stick(px(1), part(-1, 4));
    return stick(fix32{}, fix32{});
}

inline void third_at_the_far_end(uint32_t t, BodyPool& pool) {
    if (t == 0) pool.spawn(at(600, 216));
}

const Scenario SCENARIOS[] = {
    {"walk-band-edges", &WALKER, 180, walk, no_event},
    {"slide-wall-corner", &WALKER, 120, corner, no_event},
    {"hop-arcs", &HEAVY, 160, hop, no_event},
    {"stick-overdrive", &HEAVY, 90, overdrive, no_event},
    {"join-leave-spawn", &WALKER, 120, join_late, leave_and_spawn},
    {"band-x-ends", &WALKER, 60, to_the_ends, third_at_the_far_end},
};

} // namespace framework::brawl::scenario
