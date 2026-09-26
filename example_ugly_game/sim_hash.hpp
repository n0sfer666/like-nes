#pragma once
#include "world.hpp"

namespace game {

// Канонический хеш боевого состояния (GameState + сущности с EntId в порядке seq) —
// golden для регресс-теста детерминизма. Фон-звёзды не хешируются (не gameplay).
uint64_t sim_hash(flecs::world& world, const GameState& gs);

} // namespace game
