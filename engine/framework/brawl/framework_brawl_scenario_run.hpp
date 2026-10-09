#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "body_hash.hpp"
#include "body_pool.hpp"
#include "framework_brawl_hit_fixture.hpp"
#include "hash_mix.hpp"

namespace framework::brawl::scenario {

constexpr fix32 px(int32_t v) { return fix32::from_int(v); }
constexpr fix32 part(int32_t num, int32_t den) { return fix32::from_raw(fix32::ONE * num / den); }

const DepthProfile WALKER{px(2), px(1), part(1, 2), px(6)};
const DepthProfile HEAVY{part(3, 2), part(3, 4), part(3, 4), px(7)};

inline DepthFloor stage() {
    DepthFloor f;
    f.band = FloorRect{px(0), px(192), px(640), px(224)};
    f.add_wall(FloorRect{px(200), px(200), px(240), px(224)});
    f.add_wall(FloorRect{px(400), px(192), px(410), px(208)});
    return f;
}

inline BrawlInput stick(fix32 mx, fix32 mz, uint16_t buttons = 0) {
    BrawlInput in;
    in.move.move_x = mx;
    in.move.move_z = mz;
    in.buttons = buttons;
    in.present = true;
    return in;
}

inline Body at(int32_t x, int32_t z) {
    Body b;
    b.pos.x = px(x);
    b.pos.z = px(z);
    b.hp = 100;
    return b;
}

inline Body foe(int32_t x, int32_t z) {
    Body b = at(x, z);
    b.team = 1;
    b.facing = -1;
    return b;
}

inline const char* STOMP_ROW = "move | stomp | hit0\nclip | jab\n"
                               "type | light\ndamage | 3\ndepth | 4\nhitstop | 1\nhitstun | 4\n"
                               "knock_x | 0\nknock_y | 0\nhits_down | yes\nslide | no\n";

enum class Move : uint8_t { None, Jab, Flurry, RunJab, Grab, Stomp };

using Script = BrawlInput (*)(uint32_t tick, uint32_t body);
using Event = void (*)(uint32_t tick, BodyPool& pool);
using Moves = Move (*)(uint32_t tick, uint32_t body);

inline Move no_moves(uint32_t, uint32_t) { return Move::None; }

struct Scenario {
    const char* name;
    const DepthProfile* profile;
    uint32_t ticks;
    Script script;
    Event event;
    Moves moves = no_moves;
};

inline void no_event(uint32_t, BodyPool&) {}

inline uint16_t named(const test::Arena& a, const char* name) {
    uint16_t row = NO_STRIKE;
    return find_named(a.kinds[0], name, row) ? row : NO_STRIKE;
}

inline uint16_t row_of(const test::Arena& a, Move m) {
    if (m == Move::Jab) return test::strike(a, a.jab).strike;
    if (m == Move::Flurry) return test::strike(a, a.flurry).strike;
    if (m == Move::RunJab) return named(a, "run_jab");
    if (m == Move::Grab) return named(a, "grab");
    return m == Move::Stomp ? named(a, "stomp") : NO_STRIKE;
}

inline uint64_t run(const Scenario& s, uint32_t* hits = nullptr) {
    test::Arena arena{test::dummy_clips(), std::string(test::DUMMY_TEXT) + test::GRAB_ROWS + STOMP_ROW};
    arena.kinds[0].profile = *s.profile;
    arena.floor = stage();
    BodyPool pool;
    pool.spawn(at(16, 208));
    pool.spawn(at(180, 196));
    uint64_t h = physics::FNV_OFFSET;
    std::vector<Command> commands;
    HitEvents events;
    for (uint32_t t = 0; t < s.ticks; ++t) {
        s.event(t, pool);
        commands.assign(pool.count, Command{});
        for (uint32_t i = 0; i < pool.count; ++i) {
            const uint32_t seq = pool.bodies[i].id.seq;
            commands[i] = Command{s.script(t, seq), row_of(arena, s.moves(t, seq))};
        }
        step_brawl(pool, commands, arena.world(), events);
        physics::mix_u64(h, state_hash(pool));
        physics::mix_u64(h, events.count);
        if (hits != nullptr) *hits += events.count;
    }
    return arena.error.empty() ? h : 0;
}

} // namespace framework::brawl::scenario
