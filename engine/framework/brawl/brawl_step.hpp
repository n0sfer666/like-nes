#pragma once
#include <cstdint>
#include <span>

#include "archetype.hpp"
#include "body_pool.hpp"
#include "brawl_input.hpp"
#include "depth_floor.hpp"
#include "hit_events.hpp"
namespace framework::scene {
struct LevelObjects;
}

namespace framework::brawl {

struct PlatformWorld;
struct SectionBody;
struct StageSections;
struct StageRopes;

struct Command {
    BrawlInput input;
    uint16_t strike = NO_STRIKE;
};

struct BrawlWorld {
    std::span<const Archetype> kinds;
    const DepthFloor* floor = nullptr;
    HitRules rules;
    const scene::LevelObjects* level = nullptr;
    const PlatformWorld* platform = nullptr;
};

// `platform` — запись тела в platform: его движение ведёт контроллер #16 мира `w.platform`, FSM та же.
// `b.crushed` — факт тика: сбрасывается здесь, поднимают его контроллер и поводок автоскролла.
// `roped` — седок троса: его в этом тике уже сдвинул трос (`step_ropes`), контроллер не шагает.
void step_one(Body& b, const Command& cmd, const BrawlWorld& w, SectionBody* platform = nullptr, bool roped = false);

// Тик драки — тела, затем удары. Разрезан на две половины ради сим-камеры (`step_stage`): её поводок
// двигает тела после движения и ДО сбора ударов, и удар обязан судить уже зажатую позицию.
// `sections == nullptr` — уровень без платформенных секций, все тела в belt; `ropes == nullptr` — без тросов.
void step_bodies(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, StageSections* sections,
                 const StageRopes* ropes = nullptr);
void resolve_hits(BodyPool& pool, const BrawlWorld& w, const StageSections* sections, HitEvents& events);
void step_brawl(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, HitEvents& events);

} // namespace framework::brawl
