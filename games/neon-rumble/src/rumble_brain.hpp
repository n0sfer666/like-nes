#pragma once
#include <cstdint>
#include <span>

#include "ai_step.hpp"

namespace rumble {

constexpr uint8_t FOE_START = 0;

// Мозг врага волны: без жетона держит кольцо вокруг ближайшего игрока, пересчитывая его каждый тик и
// уходя с его линии по z; с жетоном сближается с той же целью и бьёт, пока жетон не истёк или пока
// его самого не ударили.
std::span<const framework::ai::Think> foe_brain();

} // namespace rumble
