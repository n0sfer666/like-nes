#include "hit_apply.hpp"

#include <algorithm>

#include "body_clip.hpp"

namespace framework::brawl {

namespace {

struct Taken {
    int64_t damage = 0;
    uint16_t hitstop = 0;
    const HitEvent* top = nullptr;
};

uint16_t ticks(uint32_t v) { return static_cast<uint16_t>(std::min<uint32_t>(v, 0xffffu)); }

const Strike& strike_of(const HitEvent& e, std::span<const Archetype> kinds) {
    return kinds[e.kind].moves[e.move].strike;
}

void remember(BodyPool& pool, std::span<const Archetype> kinds, HitEvents& events,
              std::array<bool, MAX_HIT_EVENTS>& kept) {
    for (uint32_t i = 0; i < events.count; ++i) {
        const HitEvent& e = events.at[i];
        Body* a = pool.find(e.attacker);
        kept[i] = a != nullptr && pool.find(e.target) != nullptr && a->struck.add(e.target, e.box);
        if (!kept[i]) ++events.dropped;
        if (kept[i]) a->hitstop = std::max(a->hitstop, ticks(strike_of(e, kinds).hitstop));
    }
}

Taken taken_by(const Body& t, std::span<const Archetype> kinds, const HitEvents& events,
               const std::array<bool, MAX_HIT_EVENTS>& kept) {
    Taken out;
    for (uint32_t i = 0; i < events.count; ++i) {
        const HitEvent& e = events.at[i];
        if (!kept[i] || !(e.target == t.id)) continue;
        const Strike& s = strike_of(e, kinds);
        out.damage += s.damage;
        out.hitstop = std::max(out.hitstop, ticks(s.hitstop));
        if (out.top == nullptr || strike_of(*out.top, kinds).hitstun < s.hitstun) out.top = &e;
    }
    return out;
}

void take(Body& t, const Taken& hit, std::span<const Archetype> kinds) {
    t.hp = static_cast<int32_t>(std::max<int64_t>(int64_t{t.hp} - hit.damage, 0));
    t.hitstop = std::max(t.hitstop, hit.hitstop);
    const Strike& s = strike_of(*hit.top, kinds);
    t.pos.vx = hit.top->knock_vx;
    t.pos.vz = fix32{};
    t.pos.vy = fix32{} < s.knock_y || fix32{} < t.pos.y ? s.knock_y : fix32{};
    if (s.hitstun == 0) return;
    t.hitstun = ticks(s.hitstun);
    if (t.kind < kinds.size()) end_strike(t, kinds[t.kind]);
}

} // namespace

void apply_hits(BodyPool& pool, std::span<const Archetype> kinds, HitEvents& events) {
    std::array<bool, MAX_HIT_EVENTS> kept{};
    remember(pool, kinds, events, kept);
    for (uint32_t i = 0; i < pool.count; ++i) {
        const Taken hit = taken_by(pool.bodies[i], kinds, events, kept);
        if (hit.top != nullptr) take(pool.bodies[i], hit, kinds);
    }
}

} // namespace framework::brawl
