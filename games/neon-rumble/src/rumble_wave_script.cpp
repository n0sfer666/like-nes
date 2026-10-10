#include "rumble_wave_script.hpp"

#include "rumble_roster.hpp"
#include "rumble_steer.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;

constexpr int32_t REACH = 32;
constexpr int32_t NEAR = 2;
constexpr uint32_t PUNCH_EVERY = 6;

} // namespace

PlayerCommand WaveScript::next(const Brawl& brawl, uint32_t t) {
    PlayerCommand c;
    c.input.present = true;
    const br::Body* me = brawl.pool.find(brawl.seats.at[PLAYER].body);
    if (me == nullptr) return c;
    if (first == 0) first = me->id.seq;
    if (me->id.seq == first) return c;
    const br::Body* foe = framework::ai::nearest_body(brawl.pool, *me, ROSTER[DUMMY].team);
    if (foe == nullptr) return c;
    const int32_t s = me->pos.x < foe->pos.x ? -1 : 1;
    c.input.move.move_z = fix32::from_int(toward(me->pos.z, foe->pos.z, NEAR));
    if (framework::ai::axis_gap(me->pos.x, foe->pos.x) > fix32::from_int(REACH).raw) return c;
    if (me->facing != -s) {
        c.input.move.move_x = fix32::from_int(-s);
        return c;
    }
    if (t % PUNCH_EVERY == 0) c.attack = Attack::Punch;
    return c;
}

} // namespace rumble
