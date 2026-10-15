#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"
#include "rumble_command.hpp"
#include "rumble_roster.hpp"

namespace rumble {

// Игрок headless-прогона --wave: без врагов любое тело идёт вправо, к следующей арене, а ящик на
// пути разбивает ногой; в platform-секции встаёт на ленту, и дальше его везёт она. При врагах первое
// тело стоит, пока ИИ не доведёт его до hp 0; тело, которое step_seats ставит заново (внутри вида),
// встаёт в глубину на линию ближайшего врага и бьёт подошедшего. Скрипт играет место `seat`; с
// `goal` сошедший с конца ленты идёт вправо, пока не дойдёт до `x` goal.
struct WaveScript {
    uint32_t seat = PLAYER;
    int32_t goal = 0;
    uint32_t first = 0;

    PlayerCommand next(const Brawl& brawl, uint32_t t);
};

} // namespace rumble
