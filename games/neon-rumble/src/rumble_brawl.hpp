#pragma once
#include <array>
#include <cstdint>

#include "body_pool.hpp"
#include "depth_floor.hpp"
#include "hit_events.hpp"
#include "rumble_command.hpp"
#include "rumble_kinds.hpp"
#include "rumble_roster.hpp"

namespace rumble {

struct Level;

static_assert(FIGHTERS <= framework::brawl::POOL_CAPACITY);

using DrawOrder = std::array<uint32_t, FIGHTERS>;

struct Brawl {
    framework::brawl::DepthFloor floor;
    framework::brawl::BodyPool pool;
    Kinds kinds;
    framework::brawl::HitEvents events;
    PlayerCommand player;
    std::array<int32_t, FIGHTERS> hp_before{};

    bool open(const Level& level, const Fighters& fighters);
    void step();
    const framework::brawl::Body& body(uint32_t fighter) const { return pool.bodies[fighter]; }
    uint32_t fighter_of(framework::brawl::EntId id) const;
    DrawOrder draw_order() const;
};

} // namespace rumble
