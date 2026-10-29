#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

#include "body_hash.hpp"
#include "framework_ai_toy.hpp"
#include "framework_brawl_shot_fixture.hpp"
#include "hash_mix.hpp"
#include "seat_hash.hpp"
#include "seat_step.hpp"

namespace framework::ai::bench {

using brawl::Body;
using brawl::BrawlInput;
using brawl::Command;
using brawl::Role;

constexpr uint32_t ENEMIES = 8;
constexpr uint32_t PROPS = 4;
constexpr uint32_t SHOTS = brawl::POOL_CAPACITY - brawl::SEATS - ENEMIES - PROPS;
constexpr int32_t TOUGH = 1000000;
constexpr int32_t CRATE_HP = 30;
constexpr uint16_t SHOT_LIFE = 24;
constexpr uint8_t FIGHTER = 0, SHOT = 1, CRATE = 2;

// x P2 меняется каждый тик: опоздавший ввод не совпадает с предсказанием «как в прошлом тике», и
// откат на всю задержку случается на каждом кадре, а не только на смене фазы.
inline BrawlInput scripted(uint32_t t, uint32_t p) {
    BrawlInput in;
    in.present = true;
    const uint32_t phase = (t / (11 + 7 * p)) % 4;
    const int32_t walk = phase == 0 ? 1 : (phase == 2 ? -1 : 0);
    in.move.move_x = fix32::from_int(p == 0 ? walk : static_cast<int32_t>(t % 3) - 1);
    in.move.move_z = fix32::from_int(phase == 1 ? 1 : (phase == 3 ? -1 : 0));
    if ((t + 3 * p) % 9 < 2) in.buttons = brawl::button::ATTACK;
    return in;
}

struct Census {
    uint32_t players = 0, enemies = 0, props = 0, shots = 0;
};

// Драка на бюджете зонтика в форме Sim для rollback::Session. Бойцы не умирают (`TOUGH`): мозг
// снимается только внутри step_ai, и замена павшего врага в том же тике упёрлась бы в BRAINS_MAX.
// Ящики и снаряды добираются до бюджета в начале тика, поэтому шаг всегда идёт по полному пулу.
struct Fight {
    using Input = BrawlInput;
    struct Snapshot {
        brawl::BodyPool pool;
        brawl::Seats seats;
        AiState ai;
        uint32_t tick = 0;
    };

    std::array<brawl::Archetype, 3> kinds{};
    brawl::BodyPool pool;
    brawl::Seats seats;
    AiState ai;
    uint32_t tick = 0;
    std::array<Body, brawl::SEATS> spawns{};
    std::vector<Command> commands;
    brawl::HitEvents events;
    Census least{brawl::SEATS, ENEMIES, PROPS, SHOTS};
    uint32_t hits = 0, hits_peak = 0, hits_dropped = 0, crates_broken = 0, shots_fired = 0;

    Fight() {
        kinds[FIGHTER] = test::arena().kinds[0];
        if (!brawl::projectile_kind(brawl::test::SHOT_BOX, brawl::test::shot_strike(), kinds[SHOT]) ||
            !brawl::prop_kind(brawl::test::CRATE_BOX, fix32::from_int(4), kinds[CRATE]))
            std::abort();
        for (uint32_t p = 0; p < brawl::SEATS; ++p) {
            spawns[p] = brawl::test::dummy(100 + 40 * static_cast<int32_t>(p), 240 + 8 * static_cast<int32_t>(p), 1,
                                           0, kinds[FIGHTER]);
            spawns[p].hp = TOUGH;
        }
        ai.rng.state = 0x5eed;
        ai.tokens.capacity = 2;
        ai.tokens.timeout = 45;
        for (int32_t i = 0; i < static_cast<int32_t>(ENEMIES); ++i) {
            Body foe = brawl::test::dummy(i % 2 == 0 ? 20 + 12 * i : 240 + 12 * i, 230 + 4 * i, i % 2 == 0 ? 1 : -1,
                                          1, kinds[FIGHTER]);
            foe.hp = TOUGH;
            if (!add_brain(ai.brains, pool.spawn(foe), test::APPROACH)) std::abort();
        }
    }

    brawl::BrawlWorld world() const { return brawl::BrawlWorld{kinds, &test::arena().floor, brawl::HitRules{}}; }

    Census census() const {
        Census c;
        for (uint32_t i = 0; i < pool.count; ++i) {
            const Body& b = pool.bodies[i];
            const Role r = kinds[b.kind].role;
            if (r == Role::Projectile) ++c.shots;
            else if (r == Role::Prop) ++c.props;
            else if (b.team == 0) ++c.players;
            else ++c.enemies;
        }
        return c;
    }

    void restock(const brawl::BrawlWorld& w) {
        Census c = census();
        for (; c.props < PROPS; ++c.props) {
            Body crate;
            crate.pos.x = fix32::from_int(static_cast<int32_t>(40 + pool.next_seq * 53 % 320));
            crate.pos.z = fix32::from_int(static_cast<int32_t>(226 + pool.next_seq * 7 % 28));
            crate.team = 2;
            crate.hp = CRATE_HP;
            crate.kind = CRATE;
            if (pool.spawn(crate) == brawl::EntId{}) return;
        }
        const uint32_t n = pool.count;
        for (uint32_t i = 0; c.shots < SHOTS && i < SHOTS * n; ++i) {
            const Body& foe = pool.bodies[(tick + i) % n];
            if (kinds[foe.kind].role != Role::Fighter || foe.team != 1) continue;
            const brawl::DepthBody from{foe.pos.x, foe.pos.z, fix32{}, fix32::from_int(4 * foe.facing), fix32{}, fix32{}};
            if (brawl::spawn_projectile(pool, nullptr, foe.id, brawl::Shot{SHOT, from, SHOT_LIFE}, w) == brawl::EntId{})
                return;
            ++c.shots;
            ++shots_fired;
        }
    }

    void note(Census& at, const Census& now) {
        at.players = now.players < at.players ? now.players : at.players;
        at.enemies = now.enemies < at.enemies ? now.enemies : at.enemies;
        at.props = now.props < at.props ? now.props : at.props;
        at.shots = now.shots < at.shots ? now.shots : at.shots;
    }

    void step(const BrawlInput* row) {
        const std::span<const BrawlInput, brawl::SEATS> in(row, brawl::SEATS);
        const brawl::BrawlWorld w = world();
        brawl::drop_spent(pool, kinds, nullptr);
        brawl::step_seats(seats, pool, in, spawns);
        restock(w);
        note(least, census());
        commands.assign(pool.count, Command{});
        brawl::seat_commands(seats, pool, in, commands);
        for (Command& c : commands)
            if ((c.input.buttons & brawl::button::ATTACK) != 0) c.strike = test::jab_strike(test::arena());
        step_ai(ai, pool, w, tick, test::THINK, commands);
        events = brawl::HitEvents{};
        brawl::step_brawl(pool, commands, w, events);
        hits += events.count;
        hits_peak = events.count > hits_peak ? events.count : hits_peak;
        hits_dropped += events.dropped;
        for (uint32_t i = pool.count; i-- > 0;) {
            const Body& b = pool.bodies[i];
            if (kinds[b.kind].role == Role::Prop && b.hp <= 0 && pool.despawn(b.id)) ++crates_broken;
        }
        ++tick;
    }

    uint64_t hash() const {
        uint64_t h = brawl::state_hash(pool);
        physics::mix_u64(h, physics::seats_hash(seats));
        physics::mix(h, tick);
        physics::mix_u64(h, ai_hash(ai));
        return h;
    }

    void save(Snapshot& s) const { s = Snapshot{pool, seats, ai, tick}; }
    void restore(const Snapshot& s) {
        pool = s.pool;
        seats = s.seats;
        ai = s.ai;
        tick = s.tick;
    }
};

} // namespace framework::ai::bench
