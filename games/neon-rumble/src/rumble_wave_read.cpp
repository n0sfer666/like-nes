#include <cstdio>
#include <string>

#include "level_clamp.hpp"
#include "rumble_level.hpp"
#include "rumble_facing.hpp"
#include "rumble_floor.hpp"
#include "rumble_wave.hpp"
#include "scene_read.hpp"
#include "viewport_fit.hpp"

namespace rumble {

namespace {

namespace tl = framework::tilemap;
namespace br = framework::brawl;
namespace sc = framework::scene;

uint32_t fighter_of(std::string_view kind) {
    for (uint32_t i = 0; i < FIGHTERS; ++i)
        if (kind == ROSTER[i].fighter) return i;
    return FIGHTERS;
}

// Тело точки волны — тело спавна её бойца, перенесённое в точку, со своим `facing` и в команде врагов:
// rainbird волны — враг, хотя на спавне он боец P2.
bool foe_of(const Level& lv, const tl::ObjectMap& map, const br::DepthFloor& floor,
            const std::array<br::Body, FIGHTERS>& spawns, const sc::WaveSpawn& s, br::Body& out) {
    const char* name = tl::object_text(map, s.mark.object->name_offset);
    const uint32_t k = fighter_of(s.kind);
    if (k == FIGHTERS || k == PLAYER) {
        std::fprintf(stderr, "neon-rumble: level %s: wave spawn %s at %d,%d: kind %.*s is not a foe fighter\n", lv.name,
                     name, s.mark.at.x.to_int(), s.mark.at.y.to_int(), static_cast<int>(s.kind.size()), s.kind.data());
        return false;
    }
    out = spawns[k];
    out.team = FOE_TEAM;
    out.pos.x = s.mark.at.x;
    out.pos.z = s.mark.at.y;
    out.facing = facing_of(map, *s.mark.object);
    if (on_floor(floor, out.pos.x, out.pos.z)) return true;
    std::fprintf(stderr, "neon-rumble: level %s: wave spawn %s at %d,%d is outside the depth band or inside a wall\n",
                 lv.name, name, out.pos.x.to_int(), out.pos.z.to_int());
    return false;
}

bool waves_fit(const Level& lv, const sc::LevelObjects& level) {
    for (const sc::WaveSpawn& s : level.waves) {
        uint32_t n = 0;
        for (const sc::WaveSpawn& o : level.waves) n += o.arena == s.arena && o.wave == s.wave ? 1 : 0;
        if (n <= framework::ai::BRAINS_MAX) continue;
        std::fprintf(stderr, "neon-rumble: level %s: arena %d wave %d has %u spawns, at most %u\n", lv.name, s.arena,
                     s.wave, n, framework::ai::BRAINS_MAX);
        return false;
    }
    return true;
}

} // namespace

bool Wave::open(const Level& lv, const tl::ObjectMap& map, const br::DepthFloor& floor,
                const std::array<br::Body, FIGHTERS>& spawns) {
    std::string error;
    if (!sc::read_level_objects(map, level, error)) {
        std::fprintf(stderr, "neon-rumble: level %s: %s\n", lv.name, error.c_str());
        return false;
    }
    foes.assign(level.waves.size(), br::Body{});
    for (uint32_t i = 0; i < level.waves.size(); ++i)
        if (!foe_of(lv, map, floor, spawns, level.waves[i], foes[i])) return false;
    if (!waves_fit(lv, level)) return false;
    ai = framework::ai::AiState{};
    ai.tokens.timeout = TOKEN_TIMEOUT;
    tick = 0;
    camera = br::StageCamera{};
    const br::Body& p1 = spawns[PLAYER];
    camera.center = br::clamp_level({p1.pos.x, p1.pos.z}, br::NO_ARENA, level, framework::graphics::view_zone_half());
    sections = br::StageSections{};
    waves = br::StageWaves{};
    waves.seen = camera.center.x;
    return true;
}

} // namespace rumble
