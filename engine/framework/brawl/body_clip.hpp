#pragma once
#include "archetype.hpp"
#include "brawl_body.hpp"

namespace framework::brawl {

bool striking(const Body& b, const Archetype& a);
bool can_strike(const Body& b, const Archetype& a);
void start_strike(Body& b, uint16_t clip);
void play_clip(Body& b, uint16_t clip);
void end_strike(Body& b, const Archetype& a);
uint16_t locomotion_clip(const Body& b, const Archetype& a);
uint16_t reaction_clip(const Body& b, const Archetype& a);
uint32_t airborne_ticks(const DepthBody& d, const DepthProfile& p);
void advance_clip(Body& b, const Archetype& a);
void settle_clip(Body& b, const Archetype& a);

} // namespace framework::brawl
