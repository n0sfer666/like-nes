#pragma once
#include <array>
#include <cstdint>
#include <span>

#include "ai_state.hpp"
#include "body_pool.hpp"
#include "brawl_step.hpp"
#include "seat.hpp"

namespace rumble {

constexpr uint32_t WAVE_FOES = 3;
constexpr uint32_t WAVE_LULL = 120;
constexpr uint32_t TOKEN_TIMEOUT = 90;
constexpr std::array<uint32_t, framework::brawl::SEATS + 1> TOKENS_BY_PLAYERS{1, 1, 2};

// Мозги прошлой волны сняты шагом ИИ раньше, чем истекает пауза, поэтому места хватает всегда.
static_assert(WAVE_FOES <= framework::ai::BRAINS_MAX);
static_assert(framework::brawl::SEATS + WAVE_FOES <= framework::brawl::POOL_CAPACITY);

struct Offset {
    int32_t x = 0;
    int32_t z = 0;
};

// Строй волны от спавна adler. Настоящие волны и арены из уровня — В6.
constexpr std::array<Offset, WAVE_FOES> FORMATION{{{0, 0}, {32, -14}, {32, 10}}};

// Волна режима --wave. В хеш драки входят `ai`, `tick`, `lull` и `number`; `on` и `foes` задаёт
// open, а `cleared` и `spawned` — только для отчёта тика, как hp_before.
struct Wave {
    bool on = false;
    framework::ai::AiState ai;
    uint32_t tick = 0;
    uint32_t lull = 0;
    uint32_t number = 0;
    std::array<framework::brawl::Body, WAVE_FOES> foes{};
    bool cleared = false;
    bool spawned = false;

    bool open(framework::brawl::BodyPool& pool);
    // Зачищенная волна сменяется новой на тех же спавнах через WAVE_LULL тиков.
    void refill(framework::brawl::BodyPool& pool);
    void think(const framework::brawl::BodyPool& pool, const framework::brawl::Seats& seats,
               const framework::brawl::BrawlWorld& world, std::span<framework::brawl::Command> commands);
    uint64_t hash() const;
};

} // namespace rumble
