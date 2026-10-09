#include "hit_grab.hpp"

#include "body_react.hpp"
#include "hit_strike.hpp"

namespace framework::brawl {

namespace {

bool grabs(const HitEvent& e, std::span<const Archetype> kinds) { return strike_of(e, kinds).type == HitType::Grab; }

bool torn(std::span<const Archetype> kinds, const HitEvents& events, uint32_t g) {
    const HitEvent& e = events.at[g];
    for (uint32_t i = 0; i < events.count; ++i) {
        const HitEvent& o = events.at[i];
        if (i == g) continue;
        if (grabs(o, kinds) && o.target == e.target && !(o.attacker == e.attacker)) return true;
        if (o.target == e.attacker) return true;
    }
    return false;
}

} // namespace

void tear_grabs(std::span<const Archetype> kinds, const HitEvents& events,
                std::array<bool, MAX_HIT_EVENTS>& live) {
    for (uint32_t i = 0; i < events.count; ++i) live[i] = !grabs(events.at[i], kinds) || !torn(kinds, events, i);
}

void throw_body(Body& t, Body& thrower, const HitEvent& e, std::span<const Archetype> kinds) {
    const Strike& s = strike_of(e, kinds);
    t.pos.x = thrower.pos.x;
    t.pos.z = thrower.pos.z;
    t.pos.vx = e.knock_vx;
    t.pos.vz = fix32{};
    t.pos.vy = s.knock_y;
    t.facing = thrower.facing;
    t.grip = Grip{thrower.id, thrower.team, thrower.kind};
    start_reaction(t, Reaction::Thrown, 0, kinds[t.kind]);
    thrower.clip = kinds[thrower.kind].throws;
    thrower.elapsed = 0;
    halt_body(thrower);
}

} // namespace framework::brawl
