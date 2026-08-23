#pragma once
#include <vector>

#include "grid.hpp"

// Запросы к тайловой сетке — та же семантика, что у запросов к телам (#15, гейт 4): перекрытие
// области, трассировка луча, свип формы.
//
// Геометрия здесь НЕ пишется заново. Тайл разворачивается в `physics::WorldShape` и отвечает теми же
// `cast_shape`/`collide_shapes`, которыми отвечает мир, — потому что вторая реализация свипа
// разошлась бы с первой на округлении, и «луч видит стену, а персонаж сквозь неё проходит» стало бы
// наблюдаемо через полгода, на конкретном угле. Тот же довод, которым `cast.hpp` отказывается от
// отдельной трассировки луча.
//
// Все три ЧИТАЮЩИЕ: сетка передаётся по константной ссылке. Меняется в ней только счётчик цены, и
// пишет его шов, а не запрос напрямую.
namespace framework::tilemap {

// Какие тайлы запрос считает препятствием. Маска, а не флаг «сплошной», потому что вертикаль 3
// добавит односторонние и склоны: свип персонажа вниз обязан видеть односторонний тайл, а свип
// вверх — нет, и это разница В ЗАПРОСЕ, а не в карте.
struct TileFilter {
    TileFlags require = TILE_SOLID;
};

struct TileOverlap {
    int32_t x = 0;
    int32_t y = 0;
    // Линейный индекс тайла на карте. Он же КЛЮЧ порядка: ответ сортируется по нему, а не по
    // порядку обхода, — порядок обхода есть деталь окна, и запрос, отвечающий им, разошёлся бы сам
    // с собой при сдвиге зонда на юнит.
    uint32_t index = 0;
};

struct TileHit {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t index = 0;
    fix32 fraction;    // доля пути от начала к концу
    Vec2 point;
    Vec2 normal;       // наружу из тайла, соглашение общее с `physics::cast.hpp`
};

// Форма запроса вращается вокруг СВОЕЙ `position` — то же соглашение, что у `physics::overlap_shape`.
// `out` очищается и заполняется по возрастанию индекса тайла.
void overlap_shape(const TileGrid& g, const physics::Shape& s, Vec2 position, fix32 angle,
                   const TileFilter& f, std::vector<TileOverlap>& out);

// Ближайшее касание луча. `delta` — путь целиком, а не направление (обоснование — `physics/query.hpp`).
bool raycast(const TileGrid& g, Vec2 origin, Vec2 delta, const TileFilter& f, TileHit& out);

// Ближайшее касание формы, перенесённой на `travel`. Поворота по пути нет.
bool shapecast(const TileGrid& g, const physics::Shape& s, Vec2 position, fix32 angle, Vec2 travel,
               const TileFilter& f, TileHit& out);

} // namespace framework::tilemap
