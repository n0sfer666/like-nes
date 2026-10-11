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

struct Command {
    BrawlInput input;
    uint16_t strike = NO_STRIKE;
};

struct BrawlWorld {
    std::span<const Archetype> kinds;
    const DepthFloor* floor = nullptr;
    HitRules rules;
    const scene::LevelObjects* level = nullptr;
};

void step_one(Body& b, const Command& cmd, const BrawlWorld& w);

// Тик драки — тела, затем удары. Разрезан на две половины ради сим-камеры (`step_stage`): её поводок
// двигает тела после движения и ДО сбора ударов, и удар обязан судить уже зажатую позицию.
void step_bodies(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w);
void resolve_hits(BodyPool& pool, const BrawlWorld& w, HitEvents& events);
void step_brawl(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, HitEvents& events);

} // namespace framework::brawl
