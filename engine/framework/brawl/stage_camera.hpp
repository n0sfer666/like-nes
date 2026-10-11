#pragma once
#include <cstdint>

#include "fixmath.hpp"

namespace framework::brawl {

constexpr int16_t NO_ARENA = -1;

// Сим-камера драки: в хеше и снапшоте, поэтому без тряски. Тряска — декор `graphics::Camera`,
// которой игра отдаёт `center` для отрисовки; попади она сюда, удар по экрану менял бы, кого
// поводок прижал к краю, и ресим с другой тряской разошёлся бы по хешу.
struct StageCamera {
    Vec2 center{};
    int16_t arena = NO_ARENA;
    fix32 scroll{};
};

} // namespace framework::brawl
