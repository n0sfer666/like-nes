#include "nearest.hpp"

#include <cstdint>

namespace framework::physics {
namespace {

int64_t dot_raw(Vec2 a, Vec2 b) {
    return static_cast<int64_t>(a.x.raw) * b.x.raw + static_cast<int64_t>(a.y.raw) * b.y.raw;
}

// Частное двух величин Q32.32 в Q16.16. Числитель приходится сдвигать влево на 16, поэтому оба
// операнда сначала нормируются вправо до заведомо помещающегося порядка. Нормировка СТЕПЕНЯМИ ДВОЙКИ
// и обоих сразу: она сокращается в частном и не вносит в результат ничего, кроме потери младших
// битов, — а деление ради подгонки порядка внесло бы ошибку, зависящую от размера формы.
fix32 ratio(int64_t num, int64_t den) {
    while (den > (int64_t{1} << 46)) {
        num >>= 1;
        den >>= 1;
    }
    if (den <= 0) return fix32{};
    return fix32::from_raw(fix32::sat((num << fix32::SHIFT) / den));
}

} // namespace

Vec2 closest_on_segment(Vec2 a, Vec2 b, Vec2 p) {
    const Vec2 edge = b - a;
    const int64_t len_sq = dot_raw(edge, edge);
    if (len_sq <= 0) return a;
    const int64_t along = dot_raw(p - a, edge);
    // Концы отсекаются СРАВНЕНИЕМ, а не клампом параметра: параметр далёкой точки вылетает за
    // Q16.16 и насыщается, а насыщенное значение уже не отличить от законной единицы.
    if (along <= 0) return a;
    if (along >= len_sq) return b;
    return a + edge * ratio(along, len_sq);
}

Nearest nearest_on_core(Vec2 p, const WorldShape& s) {
    Nearest out;
    if (s.count == 1) {
        out.point = s.points[0];
    } else if (s.count == 2) {
        out.point = closest_on_segment(s.points[0], s.points[1], p);
    } else {
        // Грань наибольшего удаления. Для точки СНАРУЖИ она же и ближайшая: клампом на неё
        // накрывается и область вершины — точка, лежащая против угла, обрезается до самого угла
        // любой из двух смежных граней, и обе дают одну и ту же вершину.
        uint8_t best = 0;
        fix32 farthest = fix32::from_raw(INT32_MIN);
        for (uint8_t i = 0; i < s.count; ++i) {
            const fix32 d = dot(s.normals[i], p - s.points[i]);
            if (farthest < d) {
                farthest = d;
                best = i;
            }
        }
        out.feature = best;
        if (farthest.raw <= 0) {
            // Внутри многоугольника. Направления «из ядра» здесь нет вовсе — есть кратчайший выход,
            // и он идёт по той же грани наименьшего погружения. Выбирать дальнюю значило бы
            // протаскивать тело сквозь форму.
            out.dir = s.normals[best];
            out.dist = farthest;
            out.point = p - out.dir * farthest;
            return out;
        }
        out.point = closest_on_segment(s.points[best], s.points[(best + 1) % s.count], p);
    }
    // Длина и направление берутся ОДНИМ корнем и с запасом точности: деление на округлённую вниз
    // длину даёт направление длиннее единицы, а его ошибка входит в импульс дважды — в проекцию
    // скорости и в направление приложения.
    out.dist = normalize(p - out.point, out.dir);
    // Единственное место, где направления действительно нет: длина вышла нулевой, и нормировать
    // нечего. Ветка «точка внутри многоугольника» выше тоже возвращает нулевое расстояние на грани,
    // но с живой нормалью — и флага не ставит.
    if (out.dist.raw == 0) {
        out.dir = FALLBACK_DIR;
        out.degenerate = true;
    }
    return out;
}

} // namespace framework::physics
