#include "rumble_knives.hpp"

#include "rumble_brain.hpp"
#include "rumble_roster.hpp"

namespace rumble {

namespace br = framework::brawl;

br::Shot knife_from(const br::Body& thrower) {
    br::DepthBody at = thrower.pos;
    at.x = at.x + fix32::from_int(thrower.facing * KNIFE_FROM);
    at.y = fix32{};
    at.vx = fix32::from_int(thrower.facing * KNIFE_SPEED);
    at.vz = fix32{};
    at.vy = fix32{};
    return br::Shot{static_cast<uint8_t>(KNIFE), at, KNIFE_LIFE};
}

void throw_knives(framework::ai::Brains& brains, br::BodyPool& pool, br::StageSections& sections,
                  const br::BrawlWorld& world) {
    for (uint32_t i = 0; i < brains.count; ++i) {
        framework::ai::Brain& b = brains.at[i];
        if (b.state != SHOOTER_THROW) continue;
        b.state = SHOOTER_START;
        b.timer = KNIFE_EVERY;
        const br::Body* thrower = pool.find(b.body);
        if (thrower != nullptr) br::spawn_projectile(pool, &sections, b.body, knife_from(*thrower), world);
    }
}

} // namespace rumble
