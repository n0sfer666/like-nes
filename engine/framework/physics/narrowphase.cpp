#include "narrowphase.hpp"

namespace framework::physics {
namespace {

// Вырожденный случай — совпавшие центры — обязан дать ОПРЕДЕЛЁННУЮ нормаль, а не какую придётся:
// иначе два тела, положенные ровно друг на друга (спавн в одной точке — обычное дело), разъедутся
// в стороны, зависящие от мусора в регистрах. Вверх, потому что +Y вниз: тело выталкивается
// против гравитации, как и подсказывает интуиция.
constexpr Vec2 FALLBACK_NORMAL = {fix32{}, fix32::from_int(-1)};

bool circle_circle(const Body& a, const Body& b, Contact& out) {
    const Vec2 d = b.position - a.position;
    const fix32 sum = a.shape.radius + b.shape.radius;
    // Длина и направление берутся одним корнем и с запасом точности: деление на округлённую вниз
    // длину дало бы нормаль длиннее единицы, а её ошибка входит в импульс дважды.
    Vec2 n;
    const fix32 dist = normalize(d, n);
    if (!(dist < sum)) return false;
    out.penetration = sum - dist;
    out.normal = dist.raw > 0 ? n : FALLBACK_NORMAL;
    return true;
}

// Нормаль наружу из коробки по грани наименьшего погружения — для центра, оказавшегося ВНУТРИ.
// Наименьшего, потому что выталкивать надо кратчайшим путём: выбор дальней грани протащил бы тело
// сквозь коробку.
Vec2 escape_axis(const Vec2& p, const Aabb& box, fix32& depth) {
    const fix32 dx_min = p.x - box.min.x, dx_max = box.max.x - p.x;
    const fix32 dy_min = p.y - box.min.y, dy_max = box.max.y - p.y;
    Vec2 axis = {fix32::from_int(-1), fix32{}};
    depth = dx_min;
    if (dx_max < depth) { depth = dx_max; axis = {fix32::from_int(1), fix32{}}; }
    if (dy_min < depth) { depth = dy_min; axis = {fix32{}, fix32::from_int(-1)}; }
    if (dy_max < depth) { depth = dy_max; axis = {fix32{}, fix32::from_int(1)}; }
    return axis;
}

bool circle_box(const Body& a, const Body& b, Contact& out) {
    const Aabb bb = bounds(b);
    const Vec2 closest = {clamp_fix(a.position.x, bb.min.x, bb.max.x),
                          clamp_fix(a.position.y, bb.min.y, bb.max.y)};
    Vec2 n;
    const fix32 dist = normalize(a.position - closest, n);
    if (dist.raw > 0) {
        if (!(dist < a.shape.radius)) return false;
        out.penetration = a.shape.radius - dist;
        out.normal = -n;   // n смотрит из коробки в круг, нормаль контакта нужна из a в b
        return true;
    }
    // Центр круга внутри коробки: ближайшая точка совпала с центром, направления из неё нет.
    fix32 depth{};
    const Vec2 axis = escape_axis(a.position, bb, depth);
    out.penetration = depth + a.shape.radius;
    out.normal = {-axis.x, -axis.y};   // ось смотрит наружу (из b в a), нормаль нужна из a в b
    return true;
}

bool box_box(const Body& a, const Body& b, Contact& out) {
    const Aabb ba = bounds(a), bb = bounds(b);
    const fix32 ox = min_fix(ba.max.x, bb.max.x) - max_fix(ba.min.x, bb.min.x);
    const fix32 oy = min_fix(ba.max.y, bb.max.y) - max_fix(ba.min.y, bb.min.y);
    if (!(ox.raw > 0) || !(oy.raw > 0)) return false;
    // Ось наименьшего перекрытия. При РАВНЫХ перекрытиях выбирается X — произвольно, но
    // одинаково везде; «выбрать любую» здесь означало бы разъезжающийся golden.
    //
    // Сторона — по позициям тел, а не по вычисленным центрам AABB. Для коробки это одно и то же
    // ((p - h + p + h) / 2 == p), но вычисление делит с усечением К НУЛЮ, и около нуля две
    // коробки с суммами -1 и +1 получают одинаковый центр: нормаль уходит не в ту сторону, и
    // решатель толкает тела друг в друга вместо того, чтобы разводить.
    if (!(oy < ox)) {
        out.penetration = ox;
        out.normal = {fix32::from_int(a.position.x < b.position.x ? 1 : -1), fix32{}};
    } else {
        out.penetration = oy;
        out.normal = {fix32{}, fix32::from_int(a.position.y < b.position.y ? 1 : -1)};
    }
    return true;
}

} // namespace

bool collide(const Body& a, const Body& b, Contact& out) {
    const bool a_circle = a.shape.kind == ShapeKind::Circle;
    const bool b_circle = b.shape.kind == ShapeKind::Circle;
    if (a_circle && b_circle) return circle_circle(a, b, out);
    if (a_circle) return circle_box(a, b, out);
    if (b_circle) {
        // Коробка против круга считается тем же кодом с переставленными телами, а не вторым
        // почти-таким-же: две реализации одной геометрии разъезжаются на округлении, и разъезд
        // видно не в тесте формы, а в golden-хеше через полгода.
        if (!circle_box(b, a, out)) return false;
        out.normal = -out.normal;
        return true;
    }
    return box_box(a, b, out);
}

} // namespace framework::physics
