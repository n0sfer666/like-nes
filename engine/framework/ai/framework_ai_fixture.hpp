#pragma once
#include <array>
#include <cstdlib>
#include <cstdint>
#include <span>
#include <vector>

#include "body_hash.hpp"
#include "framework_ai_toy.hpp"
#include "hash_mix.hpp"
#include "seat_hash.hpp"
#include "seat_step.hpp"

namespace framework::ai::test {

using brawl::BrawlInput;

inline BrawlInput scripted(uint32_t t, uint32_t p) {
    BrawlInput in;
    in.present = t >= 2 + p * 30;
    const uint32_t phase = (t / (11 + 7 * p)) % 4;
    in.move.move_x = fix32::from_int(phase == 0 ? 1 : (phase == 2 ? -1 : 0));
    in.move.move_z = fix32::from_int(phase == 1 ? 1 : (phase == 3 ? -1 : 0));
    if ((t + 3 * p) % 9 < 2) in.buttons = brawl::button::ATTACK;
    return in;
}

// Драка с ИИ в форме Sim для rollback::Session. `forget` — сломанный снапшот: поле, которое он
// возвращает из живого состояния после restore, в снимок как будто не попало.
struct AiBrawl {
    using Input = BrawlInput;
    struct Snapshot {
        brawl::BodyPool pool;
        brawl::Seats seats;
        AiState ai;
        uint32_t tick = 0;
    };

    brawl::BodyPool pool;
    brawl::Seats seats;
    AiState ai;
    uint32_t tick = 0;
    std::array<Body, brawl::SEATS> spawns{};
    std::vector<Command> commands;
    brawl::HitEvents events;
    void (*forget)(AiState& restored, const AiState& live) = nullptr;
    uint32_t taken = 0, knocked_out = 0, brains_dropped = 0, tokens_orphaned = 0;

    AiBrawl() {
        const brawl::Archetype& k = arena().kinds[0];
        spawns = {brawl::test::dummy(100, 240, 1, 0, k), brawl::test::dummy(140, 248, 1, 0, k)};
        ai.rng.state = 0x5eed;
        ai.tokens.capacity = 1;
        ai.tokens.timeout = 45;
        for (int32_t i = 0; i < 3; ++i) {
            Body foe = brawl::test::dummy(220 + 40 * i, 236 + 6 * i, -1, 1, k);
            foe.hp = 90;
            if (!add_brain(ai.brains, pool.spawn(foe), APPROACH)) std::abort();
        }
    }

    void step(const BrawlInput* row) {
        const std::span<const BrawlInput, brawl::SEATS> in(row, brawl::SEATS);
        brawl::step_seats(seats, pool, in, spawns);
        commands.assign(pool.count, Command{});
        brawl::seat_commands(seats, pool, in, commands);
        for (Command& c : commands)
            if ((c.input.buttons & brawl::button::ATTACK) != 0) c.strike = jab_strike(arena());
        for (uint32_t i = 0; i < ai.tokens.count; ++i)
            if (pool.find(ai.tokens.held[i].body) == nullptr) ++tokens_orphaned;
        const uint32_t held = ai.tokens.count, brains = ai.brains.count;
        step_ai(ai, pool, arena().world(), tick, THINK, commands);
        if (ai.tokens.count > held) ++taken;
        brains_dropped += brains - ai.brains.count;
        brawl::step_brawl(pool, commands, arena().world(), events);
        for (uint32_t i = pool.count; i-- > 0;)
            if (pool.bodies[i].hp <= 0 && pool.despawn(pool.bodies[i].id)) ++knocked_out;
        ++tick;
    }

    uint64_t world_hash() const {
        uint64_t h = brawl::state_hash(pool);
        physics::mix_u64(h, brawl::seats_hash(seats));
        physics::mix(h, tick);
        return h;
    }

    uint64_t hash() const {
        uint64_t h = world_hash();
        physics::mix_u64(h, ai_hash(ai));
        return h;
    }

    void save(Snapshot& s) const { s = Snapshot{pool, seats, ai, tick}; }
    void restore(const Snapshot& s) {
        const AiState live = ai;
        pool = s.pool;
        seats = s.seats;
        ai = s.ai;
        tick = s.tick;
        if (forget != nullptr) forget(ai, live);
    }
};

} // namespace framework::ai::test
