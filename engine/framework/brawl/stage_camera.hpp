#pragma once
#include <cstdint>

#include "fixmath.hpp"

namespace framework::brawl {

constexpr int16_t NO_ARENA = -1;

// Сим-камера драки: в хеше и снапшоте, поэтому без тряски. Тряска — декор `graphics::Camera`,
// которой игра отдаёт `center` для отрисовки; попади она сюда, удар по экрану менял бы, кого
// поводок прижал к краю, и ресим с другой тряской разошёлся бы по хешу.
// `scroll` — центр вида по оси идущего автоскролла, `ride` — индекс следующего (или идущего) в
// `LevelObjects::autoscrolls`, `riding` — он едет, `settle` — по каждой оси `|speed|` кончившегося
// по ней проезда, пока центр возвращается к следованию (0 — не возвращается). Игра при открытии
// уровня ставит `center` в точку старта: старт автоскролла судит пересечение от центра прошлого шага.
struct StageCamera {
    Vec2 center{};
    int16_t arena = NO_ARENA;
    fix32 scroll{};
    uint16_t ride = 0;
    bool riding = false;
    Vec2 settle{};
};

} // namespace framework::brawl
