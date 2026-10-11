#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_wave_fixture.hpp"
#include "stage_waves_hash.hpp"

namespace {

using namespace framework::brawl;
using framework::scene::LevelObjects;
using framework::scene::WaveSpawn;
using test::area;
using test::check;
using test::dummy;
using test::level;
using test::point;
using test::Starts;
using test::Waves;

uint32_t steps_to_start(Waves& w) {
    uint32_t t = 0;
    do {
        w.step();
        ++t;
    } while (w.facts.count == 0 && t < 10);
    return t;
}

void progress() {
    Waves w(700);
    w.s.pool.spawn(dummy(650, 240, 1, 0, w.s.arena.kinds[0]));
    w.step();
    w.foe();
    w.step();
    w.step();
    check(!w.facts.cleared && w.facts.count == 0 && w.s.camera.arena == 0, "the wave moved on with a foe left");
    w.clear();
    w.step();
    check(w.facts.cleared && w.facts.count == 0 && w.waves.lull == 3, "the clear tick does not start the lull");
    check(steps_to_start(w) == 3 && w.starts() == Starts{1} && w.waves.wave == 5,
          "the next wave does not start lull ticks after the clear or keeps a wave for two");
    w.foe();
    w.step();
    w.clear();
    w.step();
    check(w.facts.cleared && w.facts.released && w.s.camera.arena == NO_ARENA && w.waves.cleared == 0 &&
              w.waves.wave == NO_WAVE,
          "the last wave does not release the lock");
    w.step();
    check(!w.facts.locked && w.s.camera.arena == NO_ARENA, "the released arena locks again");
    w.s.camera.center.x = test::at(1300);
    w.step();
    check(w.s.camera.arena == 1 && w.starts() == Starts{0}, "the next arena does not start from its first wave");

    Waves now(700);
    now.rules.lull = 0;
    now.step();
    now.clear();
    now.step();
    check(now.facts.cleared && now.starts() == Starts{1}, "a zero lull does not start the next wave in the clear tick");
}

void absent() {
    Waves none(700);
    none.s.seats.at[0].present = false;
    none.step();
    const bool waits = none.facts.locked && !none.facts.released && none.waves.wave == NO_WAVE;
    for (uint32_t t = 0; t < 5; ++t) none.step();
    check(waits && none.s.camera.arena == 0 && none.facts.count == 0, "a lock without players burns the arena's waves");
    none.s.seats.at[0].present = true;
    none.step();
    check(none.starts() == Starts{2}, "a returning player does not start the waiting wave");

    Waves left(700);
    left.step();
    left.clear();
    left.s.seats.at[0].present = false;
    for (uint32_t t = 0; t < 8; ++t) left.step();
    check(left.s.camera.arena == 0 && left.waves.wave == 0 && left.waves.cleared == NO_ARENA,
          "every seat gone in the lull releases the arena");
    left.s.seats.at[0].present = true;
    left.step();
    check(left.starts() == Starts{1} && left.waves.wave == 5, "a returning player does not start the next wave");
}

// Сломанная фикстура «масштаб текущей волны при входе»: вошедший досыпает точки идущей волны.
struct ScaleNow {
    int32_t seen = 1;

    void operator()(Waves& w) {
        w.step();
        int32_t now = 0;
        for (const Seat& s : w.s.seats.at) now += s.present ? 1 : 0;
        const bool running = w.waves.wave != NO_WAVE && w.waves.lull == 0 && w.facts.count == 0;
        for (uint32_t i = 0; running && i < w.s.level.waves.size(); ++i) {
            const WaveSpawn& p = w.s.level.waves[i];
            if (p.arena == w.s.camera.arena && p.wave == w.waves.wave && p.players > seen && p.players <= now)
                w.facts.starts[w.facts.count++] = i;
        }
        seen = now;
    }
};

template <class Step> bool scales_next(Step step) {
    Waves w(700);
    step(w);
    w.foe();
    w.s.seat(1, 650);
    uint32_t mid = 0;
    for (uint32_t t = 0; t < 3; ++t) {
        step(w);
        mid += w.facts.count;
    }
    w.clear();
    for (uint32_t t = 0; t < 4; ++t) step(w);
    return mid == 0 && w.starts() == Starts{3};
}

void scale() {
    check(scales_next([](Waves& w) { w.step(); }), "a player who joined mid-wave does not scale the next wave");
    check(!scales_next(ScaleNow{}), "the current wave scaled on join passes the judge");
    Waves two(700);
    two.s.seat(1, 650);
    two.step();
    check(two.starts() == Starts{2, 4}, "two players at the start: the wave misses a point for two");
    Waves fallen(700);
    fallen.s.pool.despawn(fallen.s.seat(1, 650).id);
    fallen.step();
    check(fallen.starts() == Starts{2, 4}, "a present seat without a body is not counted");
    LevelObjects only_two = level();
    only_two.waves[1].arena = 1;
    Waves lull(700, only_two);
    lull.step();
    lull.clear();
    lull.step();
    check(lull.facts.cleared && !lull.facts.released, "a wave for two left: the lock is released before its start");
    lull.s.seat(1, 650);
    check(steps_to_start(lull) == 3 && lull.starts() == Starts{3}, "a player who joined in the lull is not counted");
    Waves alone(700, only_two);
    alone.step();
    alone.clear();
    for (uint32_t t = 0; t < 3; ++t) alone.step();
    const bool held = !alone.facts.released && alone.s.camera.arena == 0;
    alone.step();
    check(held && alone.facts.released && alone.s.camera.arena == NO_ARENA && alone.waves.cleared == 0 &&
              alone.waves.wave == NO_WAVE && alone.facts.count == 0,
          "a wave for two left: one player is not released after the lull");
}

void overflow() {
    LevelObjects l;
    l.arenas.push_back({area(400, 0, 1000, 216), 0});
    for (uint32_t i = 0; i < POOL_CAPACITY + 2; ++i) l.waves.push_back(point(0, 0, 1));
    Waves w(700, l);
    w.step();
    check(w.facts.count == POOL_CAPACITY && w.facts.dropped == 2 && w.facts.starts[POOL_CAPACITY - 1] == POOL_CAPACITY - 1,
          "the starts past the capacity are not counted");
}

struct Field {
    const char* name;
    void (*set)(StageWaves&, int32_t);
};

const Field FIELDS[] = {
    {"cleared", [](StageWaves& w, int32_t v) { w.cleared = static_cast<int16_t>(v); }},
    {"wave", [](StageWaves& w, int32_t v) { w.wave = v; }},
    {"lull", [](StageWaves& w, int32_t v) { w.lull = static_cast<uint16_t>(v); }},
    {"seen", [](StageWaves& w, int32_t v) { w.seen = test::at(v); }},
};

void fields() {
    for (const Field& f : FIELDS) {
        StageWaves one{};
        StageWaves two{};
        f.set(one, 1);
        f.set(two, 2);
        if (waves_hash(one) == waves_hash(two)) {
            std::printf("  FAIL: waves field %s is outside the hash\n", f.name);
            ++test::fails;
        }
    }
}

} // namespace

int main() {
    progress();
    absent();
    scale();
    overflow();
    fields();
    return test::verdict("framework-brawl-wave");
}
