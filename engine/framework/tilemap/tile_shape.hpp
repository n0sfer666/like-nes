#pragma once
#include "grid.hpp"
#include "shape.hpp"

// Во что разворачивается тайл геометрией — отдельно от того, как запрос обходит окно.
//
// Граница здесь по вопросу, а не по длине: «какая у тайла форма» спрашивают и три запроса
// (`query.cpp`), и гейт, который эту форму пинит; а «как обойти окно» — деталь запроса и наружу не
// выходит. Пока форм было ровно две (коробка на каждый тайл), вопроса не существовало.
namespace framework::tilemap {

// Геометрия тайла по его флагам. Коробка строится всегда, склоны — ЛЕНИВО, при первой встрече:
// `sanitize` каждой формы стоит корней на нормаль, а карта без единого склона не обязана платить за
// четыре зеркала, которых на ней нет. Кеш живёт ровно один запрос — форма зависит от размера тайла,
// то есть от сетки, и общий на все сетки кеш пришлось бы сторожить ключом.
//
// Треугольник задан в ЛОКАЛЬНЫХ координатах тайла, вокруг его центра: `to_world` переносит его тем
// же путём, что коробку, поэтому раскладка склона нигде не считается второй раз. Обмотка неважна —
// `sanitize` строит выпуклую оболочку (`shape.cpp`).
class TileShapes {
public:
    explicit TileShapes(fix32 half)
        : half_(half), box_(physics::sanitize(physics::box(half, half))) {}

    const physics::Shape& of(TileFlags flags) {
        if ((flags & TILE_SLOPE) == 0) return box_;
        const uint32_t v = ((flags & TILE_SLOPE_FLIP_X) != 0 ? 1u : 0u) |
                           ((flags & TILE_SLOPE_FLIP_Y) != 0 ? 2u : 0u);
        if (!ready_[v]) {
            slope_[v] = build(v);
            ready_[v] = true;
        }
        return slope_[v];
    }

private:
    physics::Shape build(uint32_t variant) const {
        // Базовый склон (`slope_br`): нижний левый, нижний правый и верхний правый углы тайла, то
        // есть прямой угол внизу справа и гипотенуза через весь тайл. Зеркала — сменой знака оси, а
        // не тремя отдельными списками точек: три списка разошлись бы при первой же правке одного.
        const fix32 x = (variant & 1u) != 0 ? -half_ : half_;
        const fix32 y = (variant & 2u) != 0 ? -half_ : half_;
        const Vec2 tri[3] = {{-x, y}, {x, y}, {x, -y}};
        return physics::sanitize(physics::polygon(tri, 3));
    }

    fix32 half_;
    physics::Shape box_;
    physics::Shape slope_[4];
    bool ready_[4] = {false, false, false, false};
};

} // namespace framework::tilemap
