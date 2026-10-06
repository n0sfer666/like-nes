#pragma once
#include <span>

#include "archetype.hpp"
#include "body_pool.hpp"
#include "hit_events.hpp"

namespace framework::brawl {

using HitFilter = bool (*)(const Body& attacker, const Body& target, const HitRules& rules);

bool may_hit(const Body& attacker, const Body& target, const HitRules& rules);
graphics::ClipView body_clip(const Body& b, std::span<const Archetype> kinds);
void collect_from(const BodyPool& pool, uint32_t slot, std::span<const Archetype> kinds, const HitRules& rules,
                  HitEvents& out, HitFilter filter = may_hit);
void collect_hits(const BodyPool& pool, std::span<const Archetype> kinds, const HitRules& rules, HitEvents& out);

} // namespace framework::brawl
