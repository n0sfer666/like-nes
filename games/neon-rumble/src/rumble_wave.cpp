#include "rumble_wave.hpp"

#include "hash_mix.hpp"
#include "rumble_brain.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;
namespace ai = framework::ai;

bool spawn(const std::array<br::Body, WAVE_FOES>& foes, br::BodyPool& pool, ai::Brains& brains) {
    bool ok = true;
    for (const br::Body& f : foes) ok = ai::add_brain(brains, pool.spawn(f), FOE_START) && ok;
    return ok;
}

bool foes_left(const br::BodyPool& pool) {
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.bodies[i].team != 0) return true;
    return false;
}

uint32_t players(const br::Seats& seats) {
    uint32_t n = 0;
    for (const br::Seat& s : seats.at) n += s.present ? 1 : 0;
    return n;
}

} // namespace

bool Wave::open(br::BodyPool& pool) {
    ai = ai::AiState{};
    ai.tokens.timeout = TOKEN_TIMEOUT;
    tick = 0;
    lull = 0;
    number = 1;
    return spawn(foes, pool, ai.brains);
}

void Wave::refill(br::BodyPool& pool) {
    cleared = false;
    spawned = false;
    if (foes_left(pool)) return;
    if (lull == 0) {
        lull = WAVE_LULL;
        cleared = true;
        return;
    }
    if (--lull > 0) return;
    spawn(foes, pool, ai.brains);
    ++number;
    spawned = true;
}

void Wave::think(const br::BodyPool& pool, const br::Seats& seats, const br::BrawlWorld& world,
                 std::span<br::Command> commands) {
    ai.tokens.capacity = TOKENS_BY_PLAYERS[players(seats)];
    ai::step_ai(ai, pool, world, tick, foe_brain(), commands);
    ++tick;
}

uint64_t Wave::hash() const {
    uint64_t h = ai::ai_hash(ai);
    framework::physics::mix(h, tick);
    framework::physics::mix(h, lull);
    framework::physics::mix(h, number);
    return h;
}

} // namespace rumble
