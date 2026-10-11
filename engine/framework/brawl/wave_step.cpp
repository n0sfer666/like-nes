#include "wave_step.hpp"

#include <cstdint>
#include <utility>

#include "camera.hpp"
#include "scene_objects.hpp"
#include "section_mode.hpp"
#include "viewport_fit.hpp"

namespace framework::brawl {

namespace {

constexpr int32_t ANY_PLAYERS = INT32_MAX;

const scene::Arena* next_arena(const scene::LevelObjects& level, int16_t cleared) {
    const scene::Arena* next = nullptr;
    for (const scene::Arena& a : level.arenas)
        if (a.id > cleared && a.id <= INT16_MAX && (next == nullptr || a.id < next->id)) next = &a;
    return next;
}

bool crossed(fix32 seen, fix32 at, fix32 lo, fix32 hi, fix32 half) {
    const fix32 target = graphics::clamp_axis(at, lo, hi, half);
    return clamp_fix(target, min_fix(seen, at), max_fix(seen, at)) == target;
}

bool covers(fix32 at, fix32 lo, fix32 hi, fix32 half) {
    const fix32 a = lo + half;
    const fix32 b = hi - half;
    return clamp_fix(at, min_fix(a, b), max_fix(a, b)) == at;
}

bool reached(fix32 seen, Vec2 at, const scene::LevelRect& r, const scene::LevelObjects& level) {
    const Vec2 half = graphics::view_zone_half();
    const bool x = crossed(seen, at.x, r.x0, r.x1, half.x);
    if (mode_at(level, at.x) == scene::SectionMode::Belt) return x;
    return (x || covers(at.x, r.x0, r.x1, half.x)) && clamp_fix(at.y, r.y0, r.y1) == at.y;
}

int32_t present(const Seats& seats) {
    int32_t n = 0;
    for (const Seat& s : seats.at) n += s.present ? 1 : 0;
    return n;
}

bool foes_left(const BodyPool& pool) {
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.bodies[i].team != 0) return true;
    return false;
}

bool joins(const scene::WaveSpawn& s, int16_t arena, int32_t players) {
    return s.arena == arena && s.players <= players;
}

int32_t next_wave(const scene::LevelObjects& level, int16_t arena, int32_t after, int32_t players) {
    int32_t next = NO_WAVE;
    for (const scene::WaveSpawn& s : level.waves)
        if (joins(s, arena, players) && s.wave > after && (next == NO_WAVE || s.wave < next)) next = s.wave;
    return next;
}

void release(StageWaves& waves, StageCamera& camera, WaveFacts& out) {
    waves.cleared = camera.arena;
    waves.wave = NO_WAVE;
    camera.arena = NO_ARENA;
    out.released = true;
}

void start(StageWaves& waves, StageCamera& camera, const scene::LevelObjects& level, int32_t players,
           WaveFacts& out) {
    if (players == 0) {
        waves.lull = 1;
        return;
    }
    waves.wave = next_wave(level, camera.arena, waves.wave, players);
    if (waves.wave == NO_WAVE) {
        release(waves, camera, out);
        return;
    }
    for (uint32_t i = 0; i < level.waves.size(); ++i) {
        const scene::WaveSpawn& s = level.waves[i];
        if (s.wave != waves.wave || !joins(s, camera.arena, players)) continue;
        if (out.count < out.starts.size())
            out.starts[out.count++] = i;
        else
            ++out.dropped;
    }
}

} // namespace

void step_waves(StageWaves& waves, StageCamera& camera, const BodyPool& pool, const Seats& seats,
                const scene::LevelObjects& level, const WaveRules& rules, WaveFacts& out) {
    out = WaveFacts{};
    const fix32 seen = std::exchange(waves.seen, camera.center.x);
    const int32_t players = present(seats);
    if (camera.arena == NO_ARENA) {
        const scene::Arena* arena = next_arena(level, waves.cleared);
        if (arena == nullptr || !reached(seen, camera.center, arena->area.rect, level)) return;
        camera.arena = static_cast<int16_t>(arena->id);
        out.locked = true;
        start(waves, camera, level, players, out);
        return;
    }
    if (waves.lull > 0) {
        if (--waves.lull == 0) start(waves, camera, level, players, out);
        return;
    }
    if (foes_left(pool)) return;
    out.cleared = true;
    if (next_wave(level, camera.arena, waves.wave, ANY_PLAYERS) == NO_WAVE) {
        release(waves, camera, out);
        return;
    }
    waves.lull = rules.lull;
    if (waves.lull == 0) start(waves, camera, level, players, out);
}

} // namespace framework::brawl
