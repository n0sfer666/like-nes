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
#include "rumble_wave.hpp"

namespace rumble {

struct Level;

static_assert(FIGHTERS <= framework::brawl::POOL_CAPACITY);
static_assert(framework::brawl::SEATS == DUMMY);

// Тела на поле по глубине — слоты пула; место без игрока тела не имеет и в порядок не входит.
struct DrawOrder {
    std::array<uint32_t, framework::brawl::POOL_CAPACITY> at{};
    uint32_t count = 0;

    const uint32_t* begin() const { return at.data(); }
    const uint32_t* end() const { return at.data() + count; }
};

struct Knockout {
    framework::brawl::EntId body;
    uint8_t kind = 0;
};

// Снятые в начале тика тела с hp 0 — для отчёта, вне снапшота, как hp_before.
struct Knockouts {
    std::array<Knockout, framework::brawl::POOL_CAPACITY> at{};
    uint32_t count = 0;
};

struct Brawl {
    framework::brawl::DepthFloor floor;
    framework::brawl::BodyPool pool;
    Kinds kinds;
    framework::brawl::HitEvents events;
    framework::brawl::Seats seats;
    std::array<PlayerCommand, framework::brawl::SEATS> players{};
    std::array<framework::brawl::Body, FIGHTERS> spawns{};
    Wave wave;
    std::array<int32_t, framework::brawl::POOL_CAPACITY> hp_before{};
    std::array<framework::brawl::Reaction, framework::brawl::POOL_CAPACITY> react_before{};
    Knockouts knocked;

    // Место p играет бойцом p. P1 сидит с открытия: его тело встаёт в пул раньше манекена. С `waves`
    // вместо манекена — волна из трёх adler с ИИ.
    bool open(const Level& level, const Fighters& fighters, bool waves);
    void step();
    DrawOrder draw_order() const;
    uint64_t hash() const;
};

} // namespace rumble
