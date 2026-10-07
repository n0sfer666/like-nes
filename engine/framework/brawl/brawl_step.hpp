#pragma once
#include <cstdint>
#include <span>

#include "archetype.hpp"
#include "body_pool.hpp"
#include "brawl_input.hpp"
#include "depth_floor.hpp"
#include "hit_events.hpp"

namespace framework::brawl {

constexpr uint16_t NO_STRIKE = 0xffffu;

struct Command {
    BrawlInput input;
    uint16_t strike = NO_STRIKE;
};

struct BrawlWorld {
    std::span<const Archetype> kinds;
    const DepthFloor* floor = nullptr;
    HitRules rules;
};

void step_one(Body& b, const Command& cmd, const BrawlWorld& w);
void step_brawl(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, HitEvents& events);

} // namespace framework::brawl
