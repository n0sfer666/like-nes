#include "shape.hpp"

#include <algorithm>

namespace framework::physics {
namespace {

// Знак площади треугольника — ТОЧНЫЙ, в int64 и без сдвига. `cross()` из fixmath возвращает
// fix32, то есть сдвигает результат на 16 и насыщает: на координатах до 4096 юнитов произведение
// заведомо вылетает за Q16.16, и оболочка строилась бы по насыщенным, то есть равным, величинам —
// три разные вершины выглядели бы лежащими на одной прямой. Здесь же произведение не превышает
// 7.2e16 и помещается в int64 целиком.
int64_t turn(Vec2 o, Vec2 a, Vec2 b) {
    const int64_t ax = static_cast<int64_t>(a.x.raw) - o.x.raw;
    const int64_t ay = static_cast<int64_t>(a.y.raw) - o.y.raw;
    const int64_t bx = static_cast<int64_t>(b.x.raw) - o.x.raw;
    const int64_t by = static_cast<int64_t>(b.y.raw) - o.y.raw;
    return ax * by - ay * bx;
}

// Выпуклая оболочка обходом по монотонной цепи. Результат — обход ПРОТИВ ЧАСОВОЙ в математических
// осях (на экране, где +Y вниз, он выглядит по часовой), и от этого выбора зависит знак внешней
// нормали ниже: перепутанный даёт нормали внутрь, то есть контакт, затягивающий тело в стену.
uint8_t convex_hull(Vec2* pts, uint8_t count) {
    std::sort(pts, pts + count, [](Vec2 l, Vec2 r) {
        return l.x.raw != r.x.raw ? l.x.raw < r.x.raw : l.y.raw < r.y.raw;
    });
    Vec2 hull[MAX_VERTICES * 2];
    uint8_t k = 0;
    for (uint8_t i = 0; i < count; ++i) {
        while (k >= 2 && turn(hull[k - 2], hull[k - 1], pts[i]) <= 0) --k;
        hull[k++] = pts[i];
    }
    const uint8_t lower = static_cast<uint8_t>(k + 1);
    for (uint8_t i = count - 1; i > 0; --i) {
        while (k >= lower && turn(hull[k - 2], hull[k - 1], pts[i - 1]) <= 0) --k;
        hull[k++] = pts[i - 1];
    }
    // Последняя точка цепи совпадает с первой — она замыкает обход и в наборе вершин лишняя.
    const uint8_t n = k > 1 ? static_cast<uint8_t>(k - 1) : k;
    for (uint8_t i = 0; i < n; ++i) pts[i] = hull[i];
    return n;
}

Vec2 clamp_point(Vec2 p) {
    return {clamp_fix(p.x, -MAX_SHAPE_HALF, MAX_SHAPE_HALF),
            clamp_fix(p.y, -MAX_SHAPE_HALF, MAX_SHAPE_HALF)};
}

} // namespace

Shape circle(fix32 r) {
    Shape s;
    s.radius = r;
    s.count = 1;
    return s;
}

Shape capsule(Vec2 a, Vec2 b, fix32 r) {
    Shape s;
    s.radius = r;
    s.count = 2;
    s.points[0] = a;
    s.points[1] = b;
    return s;
}

Shape box(fix32 hx, fix32 hy) {
    const Vec2 pts[4] = {{-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy}};
    return polygon(pts, 4);
}

Shape polygon(const Vec2* pts, uint32_t n) {
    Shape s;
    s.count = static_cast<uint8_t>(n < MAX_VERTICES ? n : MAX_VERTICES);
    for (uint8_t i = 0; i < s.count; ++i) s.points[i] = pts[i];
    return s;
}

Shape sanitize(const Shape& s) {
    Shape out;
    out.radius = clamp_fix(s.radius, fix32{}, MAX_SHAPE_HALF);
    out.count = s.count < MAX_VERTICES ? s.count : MAX_VERTICES;
    for (uint8_t i = 0; i < out.count; ++i) out.points[i] = clamp_point(s.points[i]);
    // Форма без единой точки — не «пустая», а невозможная: ядро есть даже у круга. Молча
    // пропустить её значило бы выдать телу форму, которая ни с чем не пересекается никогда.
    if (out.count == 0) out.count = 1;
    if (out.count >= 3) out.count = convex_hull(out.points, out.count);
    // Оболочка схлопывает совпавшие и лежащие на одной прямой вершины, поэтому многоугольник может
    // выродиться в отрезок или точку — и это законный результат, а не ошибка: у него остаётся
    // радиус, то есть он остаётся телом с объёмом.
    if (out.count == 2 && out.points[0] == out.points[1]) out.count = 1;
    // Ядро без объёма (точка или отрезок) обязано иметь радиус: иначе пересечение с ним — событие
    // нулевой меры, и тело честно проваливается сквозь всё подряд. Порог виден на глаз (1/16
    // юнита), но не нулевой, и гейт 10 держит его как всякий другой кламп.
    if (out.count < 3) out.radius = max_fix(out.radius, MIN_SHAPE_EXTENT);

    // У точечного ядра рёбер нет вовсе, поэтому условие стоит ПЕРЕД циклом: внутри оно от `i` не
    // зависит и проверялось бы на каждой вершине заново.
    if (out.count >= 2) {
        for (uint8_t i = 0; i < out.count; ++i) {
            const Vec2 e = out.points[(i + 1) % out.count] - out.points[i];
            // Внешняя нормаль обхода против часовой — правый перпендикуляр ребра. Нормируется тем же
            // способом, что нормаль контакта: SAT сравнивает расстояния ВДОЛЬ неё, и нормаль длиной
            // 1.0001 давала бы глубину проникновения, растущую с размером формы.
            normalize({e.y, -e.x}, out.normals[i]);
        }
    }
    return out;
}

void to_world(const Shape& s, Vec2 position, Rot rot, WorldShape& out) {
    out.radius = s.radius;
    out.count = s.count;
    for (uint8_t i = 0; i < s.count; ++i) {
        out.points[i] = position + rotate(rot, s.points[i]);
        // Нормаль поворачивается, но НЕ переносится: она направление, а не точка. Прибавить к ней
        // позицию значило бы получить «нормаль», указывающую из начала мира, — и SAT сравнивал бы
        // проекции на оси, зависящие от того, где стоит тело.
        out.normals[i] = rotate(rot, s.normals[i]);
    }
}

Aabb bounds(const Shape& s, Vec2 position, Rot rot) {
    Vec2 lo = rotate(rot, s.points[0]);
    Vec2 hi = lo;
    for (uint8_t i = 1; i < s.count; ++i) {
        const Vec2 p = rotate(rot, s.points[i]);
        lo = {min_fix(lo.x, p.x), min_fix(lo.y, p.y)};
        hi = {max_fix(hi.x, p.x), max_fix(hi.y, p.y)};
    }
    const Vec2 r = {s.radius, s.radius};
    return {position + lo - r, position + hi + r};
}

fix32 reach(const Shape& s) {
    fix32 farthest{};
    // Сравнивать квадраты (обычный способ обойтись без корня) здесь нельзя: квадрат расстояния на
    // форме в 4096 юнитов насыщается, и все дальние вершины оказываются одинаково далёкими. Корень
    // на вершину — цена, которую платят один раз за всю жизнь тела.
    for (uint8_t i = 0; i < s.count; ++i) farthest = max_fix(farthest, length(s.points[i]));
    return farthest + s.radius;
}

bool overlaps(const Aabb& a, const Aabb& b) {
    // Касание границы контактом НЕ считается: `<` вместо `<=`. Иначе два тела, стоящие ровно
    // впритык, каждый кадр рождают контакт с нулевым проникновением, решатель гоняет по нему
    // нулевые импульсы, а стопка «дышит» на величину округления — и это попадает в sim-хеш.
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

} // namespace framework::physics
