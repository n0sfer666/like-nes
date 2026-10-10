#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "body_hash.hpp"
#include "framework_brawl_hit_fixture.hpp"
#include "hash_mix.hpp"
#include "seat_hash.hpp"
#include "seat_step.hpp"

namespace framework::brawl::test {

// Драка хотсита в форме Sim для replay::verify: манекен на поле с первого тика, тела игроков
// появляются и уходят по present. Кнопка ATTACK — удар jab, его выбор и есть часть ввода.
struct Hotseat {
    using Input = BrawlInput;
    struct Snap {
        BodyPool pool;
        Seats seats;
    };

    Arena arena;
    BodyPool pool;
    Seats seats;
    std::array<Body, SEATS> spawns{};
    std::vector<Command> commands;
    HitEvents events;

    Hotseat() {
        pool.spawn(dummy(328, 252, -1, 1, arena.kinds[0]));
        spawns = {dummy(100, 240, 1, 0, arena.kinds[0]), dummy(264, 240, 1, 0, arena.kinds[0])};
    }

    void step(const BrawlInput* row) {
        const std::span<const BrawlInput, SEATS> in(row, SEATS);
        step_seats(seats, pool, in, spawns);
        commands.assign(pool.count, Command{});
        seat_commands(seats, pool, in, commands);
        const uint16_t jab = strike(arena, arena.jab).strike;
        for (Command& c : commands)
            if ((c.input.buttons & button::ATTACK) != 0) c.strike = jab;
        step_brawl(pool, commands, arena.world(), events);
    }

    uint64_t hash() const {
        uint64_t h = state_hash(pool);
        physics::mix_u64(h, seats_hash(seats));
        return h;
    }

    Snap save() const { return Snap{pool, seats}; }
    void restore(const Snap& s) {
        pool = s.pool;
        seats = s.seats;
    }
};

inline BrawlInput walking(int32_t dir, uint16_t buttons = 0) {
    BrawlInput in;
    in.present = true;
    in.move.move_x = fix32::from_int(dir);
    in.buttons = buttons;
    return in;
}

} // namespace framework::brawl::test
