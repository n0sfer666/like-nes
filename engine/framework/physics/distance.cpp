#include "distance.hpp"

#include <cstdint>

#include "nearest.hpp"

namespace framework::physics {
namespace {

// Строгое «меньше», и порядок обхода фиксирован: у двух коллинеарных капсул кратчайшее расстояние
// достигается сразу на двух парах вершина-ребро, и правило «побеждает первый» — не «всё равно
// какой», а условие того, что три ОС выберут одну и ту же точку.
void consider(CoreDistance& best, const CoreDistance& c) {
    if (c.dist < best.dist) best = c;
}

} // namespace

CoreDistance core_distance(const WorldShape& a, const WorldShape& b) {
    CoreDistance best{{}, {}, fix32::from_raw(INT32_MAX), 0, false};
    for (uint8_t i = 0; i < b.count; ++i) {
        const Nearest n = nearest_on_core(b.points[i], a);
        consider(best, {n.point, n.dir, n.dist, i, false});
    }
    for (uint8_t i = 0; i < a.count; ++i) {
        const Nearest n = nearest_on_core(a.points[i], b);
        // `dir` смотрит из ядра `b` в вершину `a`, а наружу нужно из `a` в `b`. Знак разворачивается,
        // а вырожденное умолчание — нет: у совпавшей с ядром вершины направления нет вовсе, и
        // `nearest_on_core` подставляет туда мировую ось (`FALLBACK_DIR`). Пройди она через минус, и
        // разъезжающаяся пара уходила бы в РАЗНЫЕ стороны в зависимости от того, какая половина
        // перебора победила, — то есть от нумерации вершин. Спрашивается при этом именно флаг, а не
        // нулевое расстояние: вершина, легшая ровно на грань, тоже даёт ноль, но с живой нормалью
        // грани, и подмена её умолчанием была бы выброшенным правильным ответом.
        const Vec2 dir = n.degenerate ? FALLBACK_DIR : -n.dir;
        consider(best, {a.points[i], dir, n.dist, i, true});
    }
    return best;
}

} // namespace framework::physics
