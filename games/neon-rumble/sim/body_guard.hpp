#pragma once
#include "archetype.hpp"
#include "brawl_body.hpp"
#include "brawl_input.hpp"

namespace framework::brawl {

bool track_guard(Body& b, const BrawlInput& in, const Archetype& a);
bool guards(const Body& target, const Body& attacker, const Strike& s);
void guard_strike(Body& b, fix32 knock_vx, const Strike& s);

} // namespace framework::brawl
