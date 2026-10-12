#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ai_state.hpp"
#include "body_pool.hpp"
#include "brawl_step.hpp"
#include "depth_floor.hpp"
#include "hit_events.hpp"
#include "object_read.hpp"
#include "rumble_level.hpp"
#include "rumble_roster.hpp"
#include "scene_objects.hpp"
#include "seat.hpp"
#include "stage_camera.hpp"
#include "stage_sections.hpp"
#include "stage_waves.hpp"
#include "wave_facts.hpp"

namespace rumble {

constexpr uint16_t WAVE_LULL = 120;
constexpr uint32_t TOKEN_TIMEOUT = 90;
constexpr int32_t ENTRY_MARGIN = 32;
constexpr std::array<uint32_t, framework::brawl::SEATS + 1> TOKENS_BY_PLAYERS{1, 1, 2};

// Волна стартует после зачистки прошлой, поэтому тел врагов в пуле не больше одной волны, а одну
// волну не больше BRAINS_MAX держит `waves_fit` в Wave::open.
static_assert(framework::brawl::SEATS + framework::ai::BRAINS_MAX <= framework::brawl::POOL_CAPACITY);

// Режим --wave: арены и волны — объекты уровня, камера драки, замок арены и старты волн —
// фреймворка (`step_stage`, `step_waves`), тела и мозги врагов ставит игра. В хеш драки входят `ai`,
// `tick`, `camera`, `sections` и `waves`; `on`, `level` и `foes` задаёт open, а `facts`, `was_arena`,
// `was_wave` и `spawned` — только для отчёта тика, как hp_before.
struct Wave {
    bool on = false;
    framework::ai::AiState ai;
    uint32_t tick = 0;
    framework::scene::LevelObjects level;
    // Тело на каждую точку `level.waves`, в том же порядке: индексы `facts.starts` берут его отсюда.
    std::vector<framework::brawl::Body> foes;
    framework::brawl::StageCamera camera;
    framework::brawl::StageSections sections;
    framework::brawl::StageWaves waves;
    framework::brawl::WaveFacts facts;
    int16_t was_arena = framework::brawl::NO_ARENA;
    int32_t was_wave = framework::brawl::NO_WAVE;
    std::array<framework::brawl::EntId, framework::brawl::POOL_CAPACITY> spawned{};
    uint32_t spawned_count = 0;

    // Камера встаёт в точку P1, зажатую в `bounds`, и `waves.seen` — в её `x`: иначе первый шаг
    // «пересёк» бы путь от нуля и запер арену, до которой игрок не дошёл.
    bool open(const Level& lv, const framework::tilemap::ObjectMap& map, const framework::brawl::DepthFloor& floor,
              const std::array<framework::brawl::Body, FIGHTERS>& spawns);
    // `x` точек мест — в зону камеры с отступом ENTRY_MARGIN и в полосу пола: вставший заново после
    // hp 0 появляется в виде, а не за краем запертой арены, куда поводок дотащил бы его только через
    // тик. Сдвинутая точка внутри стены — место остаётся на своём спавне, проверенном при открытии.
    void entry(std::span<framework::brawl::Body, framework::brawl::SEATS> at,
               const framework::brawl::DepthFloor& floor) const;
    // ИИ → тик сцены → волны → тела и мозги стартовавших точек; не влезшие идут в `facts.dropped`.
    void step(framework::brawl::BodyPool& pool, const framework::brawl::Seats& seats,
              framework::brawl::BrawlWorld world, std::span<framework::brawl::Command> commands,
              framework::brawl::HitEvents& events);
    uint64_t hash() const;
};

} // namespace rumble
