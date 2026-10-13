#pragma once
#include <cstdint>
#include <span>

#include "archetype.hpp"
#include "body_pool.hpp"

namespace framework::ai {

int64_t axis_gap(fix32 a, fix32 b);

// Ближайший живой боец команды `team` по |dx| + |dz|; при равенстве — меньший `kind`, затем меньший
// индекс в пуле. Игра, где место p играет бойцом p, получает этим «меньшее место».
const brawl::Body* nearest_body(const brawl::BodyPool& pool, std::span<const brawl::Archetype> kinds, const brawl::Body& from,
                                uint8_t team);

} // namespace framework::ai
