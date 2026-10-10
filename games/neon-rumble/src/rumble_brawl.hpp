#pragma once
#include <array>
#include <cstdint>

#include "body_pool.hpp"
#include "depth_floor.hpp"
#include "hit_events.hpp"
#include "reaction.hpp"
#include "seat.hpp"
#include "rumble_command.hpp"
#include "rumble_kinds.hpp"
#include "rumble_roster.hpp"

namespace rumble {

struct Level;

static_assert(FIGHTERS <= framework::brawl::POOL_CAPACITY);
static_assert(framework::brawl::SEATS == DUMMY);

// Бойцы на поле по глубине: место без игрока тела не имеет и в порядок не входит.
struct DrawOrder {
    std::array<uint32_t, FIGHTERS> at{};
    uint32_t count = 0;

    const uint32_t* begin() const { return at.data(); }
    const uint32_t* end() const { return at.data() + count; }
};

struct Brawl {
    framework::brawl::DepthFloor floor;
    framework::brawl::BodyPool pool;
    Kinds kinds;
    framework::brawl::HitEvents events;
    framework::brawl::Seats seats;
    std::array<PlayerCommand, framework::brawl::SEATS> players{};
    std::array<framework::brawl::Body, FIGHTERS> spawns{};
    std::array<int32_t, FIGHTERS> hp_before{};
    std::array<framework::brawl::Reaction, FIGHTERS> react_before{};

    // Место p играет бойцом p. P1 сидит с открытия: его тело встаёт в пул раньше манекена.
    bool open(const Level& level, const Fighters& fighters);
    void step();
    const framework::brawl::Body* find(uint32_t fighter) const;
    uint32_t fighter_of(framework::brawl::EntId id) const;
    DrawOrder draw_order() const;
    uint64_t hash() const;
};

} // namespace rumble
