#pragma once
#include <array>
#include <span>

#include "archetype.hpp"
#include "body_pool.hpp"
#include "hit_events.hpp"

namespace framework::brawl {

void tear_grabs(std::span<const Archetype> kinds, const HitEvents& events,
                std::array<bool, MAX_HIT_EVENTS>& live);
void throw_body(Body& t, Body& thrower, const HitEvent& e, std::span<const Archetype> kinds);

} // namespace framework::brawl
