#pragma once
#include <span>

#include "archetype.hpp"
#include "hit_events.hpp"

namespace framework::brawl {

inline const Strike& strike_of(const HitEvent& e, std::span<const Archetype> kinds) {
    return kinds[e.kind].moves[e.move].strike;
}

} // namespace framework::brawl
