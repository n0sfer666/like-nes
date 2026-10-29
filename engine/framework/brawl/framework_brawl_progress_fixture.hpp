#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "body_hash.hpp"
#include "checkpoint_step.hpp"
#include "death_step.hpp"
#include "framework_brawl_hit_fixture.hpp"
#include "hash_mix.hpp"
#include "progress_hash.hpp"
#include "scene_objects.hpp"
#include "seat_hash.hpp"
#include "seat_step.hpp"

namespace framework::brawl::test {

constexpr int32_t PIT_X = 100;
constexpr int32_t SPAWN_X = 40;
constexpr uint16_t RESPAWN_IMMUNE = 30;
constexpr uint32_t P2_STOP = 55;

inline BrawlInput course(uint32_t t, uint32_t p) {
    BrawlInput in;
    in.present = true;
    const bool walks = p == 0 ? t >= 80 : t < P2_STOP;
    in.move.move_x = fix32::from_int(walks ? (p == 0 ? -1 : 1) : 0);
    return in;
}

// Уровень в форме Sim для rollback::Session. P1 ходит влево в яму (`x < PIT_X` — `hp` 0, код игры) и
// встаёт у взятого чекпоинта; P2 идёт вправо и встаёт до чекпоинта 1 ближе своего хода за задержку,
// поэтому при задержке его ввода предсказание доводит P2 до точки, а коррекция — нет. `forget` —
// сломанный снапшот: прогресс после restore остаётся живым, как будто в снимок не попал.
struct Course {
    using Input = BrawlInput;
    struct Snapshot {
        BodyPool pool;
        Seats seats;
        LevelProgress progress;
        uint32_t tick = 0;
    };

    Arena arena;
    BodyPool pool;
    Seats seats;
    LevelProgress progress;
    uint32_t tick = 0;
    scene::LevelObjects level;
    std::array<Body, SEATS> spawns{};
    std::vector<Command> commands;
    HitEvents events;
    Living living;
    DeathFacts deaths;
    bool forget = false;
    uint32_t died = 0, respawned_at_one = 0;
    int16_t peak = NO_CHECKPOINT;

    Course() {
        level.checkpoints = {{{nullptr, {fix32::from_int(150), fix32::from_int(240)}}, 0},
                             {{nullptr, {fix32::from_int(300), fix32::from_int(240)}}, 1}};
        spawns = {dummy(160, 240, 1, 0, arena.kinds[0]), dummy(180, 260, 1, 0, arena.kinds[0])};
    }

    void respawn(Body& b) {
        const scene::Checkpoint* c = find_checkpoint(level, progress.checkpoint);
        b.pos = DepthBody{};
        b.pos.x = c != nullptr ? c->mark.at.x : fix32::from_int(SPAWN_X);
        b.pos.z = fix32::from_int(240);
        b.hp = 100;
        b.immune = RESPAWN_IMMUNE;
        if (progress.checkpoint == 1) ++respawned_at_one;
    }

    void step(const BrawlInput* row) {
        const std::span<const BrawlInput, SEATS> in(row, SEATS);
        step_seats(seats, pool, in, spawns);
        note_living(pool, living);
        commands.assign(pool.count, Command{});
        seat_commands(seats, pool, in, commands);
        step_brawl(pool, commands, arena.world(), events);
        for (uint32_t i = 0; i < pool.count; ++i)
            if (pool.bodies[i].pos.x < fix32::from_int(PIT_X)) pool.bodies[i].hp = 0;
        step_checkpoints(progress, pool, seats, level);
        step_deaths(pool, living, deaths);
        for (uint32_t i = 0; i < deaths.count; ++i) {
            ++died;
            respawn(*pool.find(deaths.at[i].body));
        }
        if (peak < progress.checkpoint) peak = progress.checkpoint;
        ++tick;
    }

    uint64_t world_hash() const {
        uint64_t h = state_hash(pool);
        physics::mix_u64(h, physics::seats_hash(seats));
        physics::mix(h, tick);
        return h;
    }

    uint64_t hash() const {
        uint64_t h = world_hash();
        physics::mix_u64(h, progress_hash(progress));
        return h;
    }

    void save(Snapshot& s) const { s = Snapshot{pool, seats, progress, tick}; }
    void restore(const Snapshot& s) {
        const LevelProgress live = progress;
        pool = s.pool;
        seats = s.seats;
        progress = forget ? live : s.progress;
        tick = s.tick;
    }
};

} // namespace framework::brawl::test
