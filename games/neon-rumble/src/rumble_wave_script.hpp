#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"
#include "rumble_command.hpp"

namespace rumble {

// Игрок headless-прогона --wave: без врагов любое тело идёт вправо, к следующей арене. При врагах
// первое тело стоит, пока ИИ не доведёт его до hp 0; тело, которое step_seats ставит заново (внутри
// вида), встаёт в глубину на линию ближайшего врага и бьёт подошедшего.
struct WaveScript {
    uint32_t first = 0;

    PlayerCommand next(const Brawl& brawl, uint32_t t);
};

} // namespace rumble
