#pragma once
#include "archetype.hpp"
#include "brawl_body.hpp"

namespace framework::brawl {

bool chain_root(const Archetype& a, uint16_t clip);
void queue_strike(Body& b, uint16_t strike, const Archetype& a);
bool take_strike(Body& b, const Archetype& a);

} // namespace framework::brawl
