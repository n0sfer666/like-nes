#pragma once
#include <span>

#include "archetype.hpp"
#include "body_pool.hpp"
#include "hit_events.hpp"

namespace framework::brawl {

void apply_hits(BodyPool& pool, std::span<const Archetype> kinds, HitEvents& events);

} // namespace framework::brawl
