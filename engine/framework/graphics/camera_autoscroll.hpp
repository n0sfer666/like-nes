#pragma once
#include <cstdint>

#include "fixmath.hpp"

namespace framework::graphics {

enum class ScrollAxis : uint8_t { X, Y };

// Дорожка уже выражена в центрах вида: размеры экрана и границы уровня определяет потребитель.
struct ScrollLane {
    ScrollAxis axis = ScrollAxis::X;
    fix32 start, end, speed;
};

struct Autoscroll {
    fix32 scroll{};
    uint16_t ride = 0;
    bool riding = false;
    Vec2 settle{};
};

// Доля возврата задаётся потребителем, ноль зажимается до единицы. Нулевая ставка оси не меняет центр.
void settle_autoscroll(Autoscroll& state, Vec2& center, Vec2 before, Vec2 goal, uint32_t share);
// `ride` выбирает дорожку у потребителя; успешный тик двигает центр, в том числе на её конце.
// Знак speed должен вести от start к end. Нулевая скорость пропускает ровно одну дорожку за тик.
bool ride_autoscroll(Autoscroll& state, Vec2& center, Vec2 before, const ScrollLane& lane);

} // namespace framework::graphics
