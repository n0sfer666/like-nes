#pragma once
#include <cstdint>

#include "framework_brawl_scenario_run.hpp"

namespace framework::brawl::scenario {

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

inline void replace(BodyPool& pool, uint32_t slot, Body b) {
    b.id = pool.bodies[slot].id;
    pool.bodies[slot] = b;
}

inline void face_off(uint32_t t, BodyPool& pool) {
    if (t == 0) replace(pool, 1, foe(40, 208));
}

inline BrawlInput close_in(uint32_t t, uint32_t body) {
    if (body == 1) return stick(px(1), fix32{});
    return stick(t % 30 < 5 ? px(-1) : fix32{}, fix32{});
}

inline Move trade_jabs(uint32_t t, uint32_t body) {
    if (body == 1) return t % 12 == 0 ? Move::Jab : Move::None;
    return t % 15 == 0 ? Move::Jab : Move::None;
}

inline Move chain_jabs(uint32_t t, uint32_t body) { return body == 1 && t % 3 == 0 ? Move::Jab : Move::None; }

inline void crowd(uint32_t t, BodyPool& pool) {
    if (t != 0) return;
    replace(pool, 0, at(100, 208));
    replace(pool, 1, foe(118, 210));
    pool.spawn(foe(120, 213));
    pool.spawn(foe(116, 217));
    pool.spawn(foe(122, 208));
}

inline BrawlInput crowd_moves(uint32_t, uint32_t body) {
    return body == 5 ? stick(fix32{}, fix32{}, button::JUMP) : stick(fix32{}, fix32{});
}

inline Move flurries(uint32_t t, uint32_t body) { return body == 1 && (t == 2 || t == 40) ? Move::Flurry : Move::None; }

inline void far_foe(uint32_t t, BodyPool& pool) {
    if (t == 0) replace(pool, 1, foe(150, 208));
}

inline BrawlInput dash(uint32_t t, uint32_t body) {
    if (body == 2) return stick(fix32{}, fix32{});
    const bool gap = t == 1 || t == 2 || (t >= 60 && t < 64) || (t >= 65 && t < 70) || t == 71;
    const fix32 mx = gap ? fix32{} : t < 60 ? px(1) : px(-1);
    return stick(mx, fix32{}, t == 80 ? button::JUMP : uint16_t{0});
}

inline Move run_jabs(uint32_t t, uint32_t body) { return body == 1 && t == 36 ? Move::RunJab : Move::None; }

const Scenario SCENARIOS[] = {
    {"walk-band-edges", &WALKER, 180, walk, no_event},
    {"slide-wall-corner", &WALKER, 120, corner, no_event},
    {"hop-arcs", &HEAVY, 160, hop, no_event},
    {"stick-overdrive", &HEAVY, 90, overdrive, no_event},
    {"join-leave-spawn", &WALKER, 120, join_late, leave_and_spawn},
    {"band-x-ends", &WALKER, 60, to_the_ends, third_at_the_far_end},
    {"jab-exchange", &WALKER, 150, close_in, face_off, trade_jabs},
    {"flurry-crowd", &HEAVY, 90, crowd_moves, crowd, flurries},
    {"jab-chain", &WALKER, 120, close_in, face_off, chain_jabs},
    {"run-slide", &WALKER, 120, dash, far_foe, run_jabs},
};

} // namespace framework::brawl::scenario
