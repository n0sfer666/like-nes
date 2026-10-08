#pragma once
#include "archetype.hpp"
#include "brawl_body.hpp"

namespace framework::brawl {

bool vulnerable(const Body& b);
void halt_body(Body& b);
void start_reaction(Body& b, Reaction r, uint16_t ticks, const Archetype& a);
void react_to(Body& b, const Strike& s, const Archetype& a);
void tick_reaction(Body& b, const Archetype& a);

} // namespace framework::brawl
