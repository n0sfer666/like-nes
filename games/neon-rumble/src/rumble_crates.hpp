#pragma once
#include <cstdint>

#include "body_pool.hpp"
#include "brawl_floor.hpp"
#include "object_read.hpp"
#include "rumble_level.hpp"

namespace rumble {

constexpr int32_t CRATE_HP = 20;
// Ящик ничей: его бьют и игроки, и враги, а `friendly_fire` выключен.
constexpr uint8_t CRATE_TEAM = 2;

// Ящики --wave — точки класса `crate`, тела вида CRATE с CRATE_HP; точка вне пола отказывает уровню.
bool place_crates(const Level& level, const framework::tilemap::ObjectMap& map,
                  const framework::brawl::DepthFloor& floor, framework::brawl::BodyPool& pool);

} // namespace rumble
