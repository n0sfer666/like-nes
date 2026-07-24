#pragma once
#include <cstdint>
#include <vector>

namespace game {

// Визуальные события боя — ПОБОЧНЫЙ выход combat_step (НЕ часть sim-состояния, НЕ в sim-hash).
// Лёгкий POD-заголовок (без GPU-зависимостей) → combat/sim (в т.ч. headless game_sim_test)
// включают только его; рендер-слой (fx.hpp) дренажит события в частицы/звук.
enum FxKind : uint8_t { FX_Fire = 0, FX_EnemyDie = 1, FX_BossHit = 2, FX_BossDie = 3, FX_PlayerHit = 4 };
struct FxEvent { float x, y; uint8_t kind; };
struct FxSink { std::vector<FxEvent> events; };

} // namespace game
