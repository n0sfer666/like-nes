#pragma once
#include "world.hpp"
#include "input_types.hpp"
#include "fx_events.hpp"

namespace game {

// Боевой шаг (S7): стрельба игрока (кулдаун), волны врагов (детерм. LCG), движение
// пуль/врагов, AABB-коллизии, счёт/жизни. Всё в целочисл./fix32 домене → детерминизм.
// fx (nullable) — побочный выход визуальных событий; sim идентичен при fx==nullptr.
void combat_step(flecs::world& world, GameState& gs, const input::InputFrame& in, fix32 dt,
                 FxSink* fx = nullptr);

// Рестарт забега (S8): очистить бой, вернуть корабль/GameState в PH_Play.
void reset_run(flecs::world& world, GameState& gs);

// Канонический хеш боевого состояния (GameState + сущности с EntId в порядке seq) —
// golden для регресс-теста детерминизма. Фон-звёзды не хешируются (не gameplay).
uint64_t sim_hash(flecs::world& world, const GameState& gs);

} // namespace game
