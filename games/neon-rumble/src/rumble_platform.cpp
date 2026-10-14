#include "rumble_platform.hpp"

#include <cstdio>

#include "next_arena.hpp"
#include "shape.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;
namespace sc = framework::scene;

constexpr int32_t HULL_HALF_W = 8;
constexpr int32_t HULL_HALF_H = 32;
constexpr int32_t TICK_HZ = 60;
constexpr int32_t DEAD_Y = 24;

bool has_platform(const sc::LevelObjects& level) {
    for (const sc::Section& s : level.sections)
        if (s.mode == sc::SectionMode::Platform) return true;
    return false;
}

} // namespace

bool Platform::open(const Level& lv, const sc::LevelObjects& level) {
    movers.reset();
    ropes.reset();
    world.reset();
    if (!has_platform(level)) return true;
    if (!lv.collision) {
        std::fprintf(stderr, "neon-rumble: level %s: a platform section needs a collision layer\n", lv.name);
        return false;
    }
    movers = std::make_unique<br::StageMovers>(level);
    ropes.emplace(level);
    br::PlatformWorld& p = world.emplace();
    p.scene = framework::character::CollisionScene{&movers->world, &*lv.collision};
    p.hull.shape = framework::physics::sanitize(
        framework::physics::box(fix32::from_int(HULL_HALF_W), fix32::from_int(HULL_HALF_H)));
    p.profile = framework::character::default_profile();
    p.dt = fix32::from_int(1) / fix32::from_int(TICK_HZ);
    p.derived = framework::character::derive(p.profile, p.dt);
    p.dead_y = fix32::from_int(DEAD_Y);
    return true;
}

// docs:begin(platform-reach)
br::DepthFloor Platform::reach(const br::DepthFloor& floor, const sc::LevelObjects& level, int16_t cleared) const {
    br::DepthFloor out = floor;
    if (!world) return out;
    if (br::next_arena(level, cleared) != nullptr) return out;
    for (const sc::Section& s : level.sections)
        if (s.mode == sc::SectionMode::Platform) out.band.x1 = framework::max_fix(out.band.x1, s.area.rect.x1);
    return out;
}
// docs:end(platform-reach)

} // namespace rumble
