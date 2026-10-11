#pragma once
#include <cstdint>
#include <utility>
#include <vector>

#include "framework_brawl_stage_fixture.hpp"
#include "wave_step.hpp"

namespace framework::brawl::test {

using Starts = std::vector<uint32_t>;

inline scene::WaveSpawn point(int32_t arena, int32_t wave, int32_t players) {
    return scene::WaveSpawn{{}, arena, wave, {}, players};
}

// Арена 0 — вид от 592 до 808, арена 1 уже вида — только середина 1300. Волна 3 — для двоих.
inline scene::LevelObjects level() {
    scene::LevelObjects l;
    l.arenas.push_back({area(1200, 0, 1400, 216), 1});
    l.arenas.push_back({area(400, 0, 1000, 216), 0});
    l.waves = {point(1, 0, 1), point(0, 5, 1), point(0, 0, 1), point(0, 3, 2), point(0, 0, 2)};
    return l;
}

struct Waves {
    Stage s;
    StageWaves waves;
    WaveFacts facts;
    WaveRules rules{3};

    explicit Waves(int32_t x, scene::LevelObjects l = level()) {
        s.level = std::move(l);
        s.camera.center.x = at(x);
        waves.seen = s.camera.center.x;
        s.seat(0, x);
    }

    void step() { step_waves(waves, s.camera, s.pool, s.seats, s.level, rules, facts); }

    void foe() { s.pool.spawn(dummy(700, 240, -1, 1, s.arena.kinds[0])); }

    void clear() {
        for (uint32_t i = s.pool.count; i-- > 0;)
            if (s.pool.bodies[i].team != 0) s.pool.despawn(s.pool.bodies[i].id);
    }

    Starts starts() const { return Starts(facts.starts.begin(), facts.starts.begin() + facts.count); }
};

} // namespace framework::brawl::test
