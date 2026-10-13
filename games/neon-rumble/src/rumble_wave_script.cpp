#include "rumble_wave_script.hpp"

#include "body_kind.hpp"
#include "rumble_roster.hpp"
#include "rumble_steer.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;

constexpr int32_t REACH = 32;
constexpr int32_t NEAR = 2;
constexpr uint32_t PUNCH_EVERY = 6;

const br::Body* crate_ahead(const Brawl& brawl, const br::Body& me) {
    const br::Body* best = nullptr;
    for (uint32_t i = 0; i < brawl.pool.count; ++i) {
        const br::Body& b = brawl.pool.bodies[i];
        if (b.hp <= 0 || br::role_of(b, brawl.kinds.types) != br::Role::Prop || b.pos.x < me.pos.x) continue;
        if (best == nullptr || b.pos.x < best->pos.x) best = &b;
    }
    return best;
}

// Ящик бьётся ногой: кулак проходит над его боксом.
PlayerCommand kick_crate(const br::Body& me, const br::Body& crate, uint32_t t) {
    PlayerCommand c;
    c.input.present = true;
    c.input.move.move_z = fix32::from_int(toward(me.pos.z, crate.pos.z, NEAR));
    if (me.facing < 0 || framework::ai::axis_gap(me.pos.x, crate.pos.x) > fix32::from_int(REACH).raw) {
        c.input.move.move_x = fix32::from_int(1);
        return c;
    }
    if (t % PUNCH_EVERY == 0 && framework::ai::axis_gap(me.pos.z, crate.pos.z) <= fix32::from_int(NEAR).raw)
        c.attack = Attack::Kick;
    return c;
}

} // namespace

PlayerCommand WaveScript::next(const Brawl& brawl, uint32_t t) {
    PlayerCommand c;
    c.input.present = true;
    const br::Body* me = brawl.pool.find(brawl.seats.at[PLAYER].body);
    if (me == nullptr) return c;
    if (first == 0) first = me->id.seq;
    const br::Body* foe = framework::ai::nearest_body(brawl.pool, brawl.kinds.types, *me, ROSTER[DUMMY].team);
    if (foe == nullptr) {
        const br::Body* crate = crate_ahead(brawl, *me);
        if (crate != nullptr) return kick_crate(*me, *crate, t);
        c.input.move.move_x = fix32::from_int(1);
        return c;
    }
    if (me->id.seq == first) return c;
    const int32_t s = me->pos.x < foe->pos.x ? -1 : 1;
    c.input.move.move_z = fix32::from_int(toward(me->pos.z, foe->pos.z, NEAR));
    // Стрелок держит кольцо и к стоящему не подходит: его скрипт догоняет, прижимая к краю арены.
    if (framework::ai::axis_gap(me->pos.x, foe->pos.x) > fix32::from_int(REACH).raw) {
        if (foe->kind == SHOOTER) c.input.move.move_x = fix32::from_int(-s);
        return c;
    }
    if (me->facing != -s) {
        c.input.move.move_x = fix32::from_int(-s);
        return c;
    }
    if (t % PUNCH_EVERY == 0) c.attack = Attack::Punch;
    return c;
}

} // namespace rumble
