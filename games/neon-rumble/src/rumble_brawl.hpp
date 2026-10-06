#pragma once
#include <array>
#include <cstdint>

#include "body_pool.hpp"
#include "brawl_input.hpp"
#include "depth_floor.hpp"
#include "depth_profile.hpp"
#include "rumble_roster.hpp"

namespace rumble {

struct Level;

static_assert(FIGHTERS <= framework::brawl::POOL_CAPACITY);

using DrawOrder = std::array<uint32_t, FIGHTERS>;

struct Brawl {
    static const framework::brawl::DepthProfile PROFILE;

    framework::brawl::DepthFloor floor;
    framework::brawl::BodyPool pool;
    framework::brawl::BrawlInput player;

    bool open(const Level& level);
    void step();
    const framework::brawl::Body& body(uint32_t fighter) const { return pool.bodies[fighter]; }
    DrawOrder draw_order() const;
};

} // namespace rumble
