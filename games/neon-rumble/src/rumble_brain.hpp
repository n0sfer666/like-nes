#pragma once
#include <cstdint>
#include <span>

#include "ai_step.hpp"

namespace rumble {

constexpr uint8_t FOE_START = 0;
constexpr uint8_t SHOOTER_START = 2;
constexpr uint8_t SHOOTER_THROW = 3;

// Мозг врага волны: без жетона держит кольцо вокруг ближайшего игрока, пересчитывая его каждый тик и
// уходя с его линии по z; с жетоном сближается с той же целью и бьёт, пока жетон не истёк или пока
// его самого не ударили.
// Стрелок (вид SHOOTER) жетона не берёт: держит кольцо на линии цели по z и, встав на неё лицом к
// цели с остывшим таймером, переходит в SHOOTER_THROW — нож выпускает `throw_knives`, у мозга нет
// пула на запись.
std::span<const framework::ai::Think> foe_brain();
uint8_t foe_start(uint8_t kind);

} // namespace rumble
