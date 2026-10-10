#include "rumble_brain.hpp"

#include <array>

#include "archetype.hpp"
#include "rumble_roster.hpp"
#include "rumble_steer.hpp"

namespace rumble {

namespace {

namespace ai = framework::ai;
namespace br = framework::brawl;

enum State : uint8_t { RING, CLOSE, STATES };

constexpr int32_t RING_X = 64;
constexpr int32_t RING_Z = 12;
constexpr int32_t RING_STEP = 16;
constexpr uint32_t RING_ROWS = 3;
constexpr int32_t STRIKE_X = 24;
constexpr int32_t STRIKE_Z = 2;
constexpr int32_t ARRIVE = 2;
constexpr uint32_t RETRY = 20;
constexpr uint32_t RETRY_SPREAD = 40;

int32_t side(const br::Body& self, const br::Body& target) { return self.pos.x < target.pos.x ? -1 : 1; }

void walk_to(br::Command& out, const br::Body& self, fix32 x, fix32 z) {
    out.input.move.move_x = fix32::from_int(toward(self.pos.x, x, ARRIVE));
    out.input.move.move_z = fix32::from_int(toward(self.pos.z, z, ARRIVE));
}

void wait(ai::Mind& m) { m.brain.timer = static_cast<uint16_t>(RETRY + ai::below(m.rng, RETRY_SPREAD)); }

void back_off(ai::Mind& m) {
    ai::release_token(m.tokens, m.body.id);
    m.brain.state = RING;
    m.brain.target = br::EntId{};
    wait(m);
}

void ring(ai::Mind& m, br::Command& out) {
    out.input.present = true;
    const br::Body* t = ai::nearest_body(m.pool, m.body, ROSTER[PLAYER].team);
    m.brain.target = t != nullptr ? t->id : br::EntId{};
    if (t == nullptr) return;
    if (m.brain.timer == 0) {
        if (ai::try_take_token(m.tokens, m.body.id, m.tick)) {
            m.brain.state = CLOSE;
            return;
        }
        wait(m);
    }
    // Сторона линии по чётности seq, дальность по seq % 3: у трёх соседних seq одной волны точки
    // кольца разные, даже когда двое ждущих на одной стороне линии.
    const int32_t lane = (m.body.id.seq & 1u) != 0 ? 1 : -1;
    const int32_t reach = RING_X + RING_STEP * static_cast<int32_t>(m.body.id.seq % RING_ROWS);
    walk_to(out, m.body, t->pos.x + fix32::from_int(side(m.body, *t) * reach),
            t->pos.z + fix32::from_int(lane * RING_Z));
}

void close(ai::Mind& m, br::Command& out) {
    out.input.present = true;
    const br::Body* t = m.pool.find(m.brain.target);
    if (t == nullptr || !ai::holds_token(m.tokens, m.body.id) || m.body.react != br::Reaction::None)
        return back_off(m);
    const int32_t s = side(m.body, *t);
    const fix32 x = t->pos.x + fix32::from_int(s * STRIKE_X);
    if (ai::axis_gap(m.body.pos.x, x) > fix32::from_int(ARRIVE).raw ||
        ai::axis_gap(m.body.pos.z, t->pos.z) > fix32::from_int(STRIKE_Z).raw)
        return walk_to(out, m.body, x, t->pos.z);
    if (m.body.facing != -s) {
        out.input.move.move_x = fix32::from_int(-s);
        return;
    }
    uint16_t jab = br::NO_STRIKE;
    if (m.body.kind < m.world.kinds.size() && br::find_named(m.world.kinds[m.body.kind], "jab", jab)) out.strike = jab;
}

constexpr std::array<ai::Think, STATES> THINK{ring, close};

} // namespace

std::span<const ai::Think> foe_brain() { return THINK; }

} // namespace rumble
