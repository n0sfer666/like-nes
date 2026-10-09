#include "hit_apply.hpp"

#include <algorithm>

#include "body_guard.hpp"
#include "body_react.hpp"
#include "hit_grab.hpp"
#include "hit_strike.hpp"

namespace framework::brawl {

namespace {

struct Taken {
    int64_t damage = 0;
    uint16_t hitstop = 0;
    const HitEvent* top = nullptr;
};

using Kept = std::array<bool, MAX_HIT_EVENTS>;

void remember(BodyPool& pool, std::span<const Archetype> kinds, HitEvents& events, Kept& kept) {
    for (uint32_t i = 0; i < events.count; ++i) {
        if (!kept[i]) continue;
        const HitEvent& e = events.at[i];
        Body* a = pool.find(e.attacker);
        kept[i] = a != nullptr && pool.find(e.target) != nullptr && a->struck.add(e.target, e.box);
        if (!kept[i]) ++events.dropped;
        if (kept[i]) a->hitstop = std::max(a->hitstop, ticks16(strike_of(e, kinds).hitstop));
    }
}

uint8_t rank(const Strike& s) {
    if (s.type == HitType::Grab) return 2;
    return fells(s) ? 1 : 0;
}

bool leads(const Strike& s, const Strike& top) {
    return rank(top) < rank(s) || (rank(top) == rank(s) && top.hitstun < s.hitstun);
}

Taken taken_by(const BodyPool& pool, const Body& t, std::span<const Archetype> kinds, const HitEvents& events,
               const Kept& kept, bool guarded) {
    Taken out;
    for (uint32_t i = 0; i < events.count; ++i) {
        const HitEvent& e = events.at[i];
        if (!kept[i] || !(e.target == t.id)) continue;
        const Strike& s = strike_of(e, kinds);
        const Body* a = pool.find(e.attacker);
        if (a == nullptr || guards(t, *a, s) != guarded) continue;
        out.damage += s.damage;
        out.hitstop = std::max(out.hitstop, ticks16(s.hitstop));
        if (out.top == nullptr || leads(s, strike_of(*out.top, kinds))) out.top = &e;
    }
    return out;
}

void take(BodyPool& pool, Body& t, const Taken& hit, std::span<const Archetype> kinds) {
    t.hp = static_cast<int32_t>(std::max<int64_t>(int64_t{t.hp} - hit.damage, 0));
    t.hitstop = std::max(t.hitstop, hit.hitstop);
    if (t.react == Reaction::Down) return;
    const Strike& s = strike_of(*hit.top, kinds);
    const bool grabbed = s.type == HitType::Grab && t.kind < kinds.size();
    Body* thrower = grabbed ? pool.find(hit.top->attacker) : nullptr;
    if (thrower != nullptr) {
        throw_body(t, *thrower, *hit.top, kinds);
        return;
    }
    t.pos.vx = hit.top->knock_vx;
    t.pos.vz = fix32{};
    t.pos.vy = fix32{} < s.knock_y || fix32{} < t.pos.y ? s.knock_y : fix32{};
    if (t.kind < kinds.size()) react_to(t, s, kinds[t.kind]);
    if (t.react == Reaction::Block) guard_strike(t, hit.top->knock_vx, s);
}

void take_guarded(Body& t, const Taken& hit, std::span<const Archetype> kinds) {
    t.hitstop = std::max(t.hitstop, hit.hitstop);
    guard_strike(t, hit.top->knock_vx, strike_of(*hit.top, kinds));
}

} // namespace

void apply_hits(BodyPool& pool, std::span<const Archetype> kinds, HitEvents& events) {
    Kept kept{};
    tear_grabs(kinds, events, kept);
    remember(pool, kinds, events, kept);
    for (uint32_t i = 0; i < pool.count; ++i) {
        Body& t = pool.bodies[i];
        const Taken hit = taken_by(pool, t, kinds, events, kept, false);
        const Taken guard = taken_by(pool, t, kinds, events, kept, true);
        if (hit.top != nullptr) take(pool, t, hit, kinds);
        else if (guard.top != nullptr) take_guarded(t, guard, kinds);
    }
}

} // namespace framework::brawl
