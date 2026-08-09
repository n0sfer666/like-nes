#include "narrowphase.hpp"

#include "nearest.hpp"
#include "sat.hpp"

namespace framework::physics {
namespace {

bool collide_point_core(const WorldShape& pt, Vec2 center_pt, const WorldShape& core,
                        Vec2 center_core, bool point_is_a, Manifold& out) {
    const Nearest n = nearest_on_core(pt.points[0], core);
    const fix32 separation = n.dist - pt.radius - core.radius;
    if (separation.raw > 0) return false;

    // Точка контакта — СЕРЕДИНА между поверхностями: ядра отстоят от них на свои радиусы, и взять
    // поверхность одной из форм значило бы сместить плечо на половину проникновения в её сторону.
    const Vec2 point = n.point + n.dir * (core.radius + fix32::from_raw(separation.raw / 2));

    // `dir` смотрит из ядра-опоры в точечное ядро. Нормаль манифольда нужна из `a` в `b` — значит
    // при точечном `a` её надо развернуть.
    out.normal = point_is_a ? -n.dir : n.dir;
    out.count = 1;
    ManifoldPoint& m = out.points[0];
    m = ManifoldPoint{};
    m.anchor_a = point - (point_is_a ? center_pt : center_core);
    m.anchor_b = point - (point_is_a ? center_core : center_pt);
    m.penetration = -separation;
    // Своим битом, а не голым номером грани: у отсечения нумерация начинается с тех же нулей, и
    // точка «вершина против грани 0» узнала бы себя в чужой точке «грань 0 об грань 0» (правило —
    // `contact.hpp`). Сегодня пара между путями не мигрирует (число вершин формы фиксируется в
    // `make_body`), и потому это стоит один бит, а не расследование.
    m.id = POINT_CORE_ID_BIT | n.feature;
    return true;
}

fix32 support(const WorldShape& s, Vec2 axis, bool maximum) {
    fix32 best = dot(axis, s.points[0]);
    for (uint8_t i = 1; i < s.count; ++i) {
        const fix32 v = dot(axis, s.points[i]);
        best = maximum ? max_fix(best, v) : min_fix(best, v);
    }
    return best;
}

// Разделены ли формы вдоль оси, которой нет среди нормалей ни одной из них. Нужно ровно для пары
// отрезков: теорема о разделяющей оси перебирает нормали граней и полна, только если хотя бы одна
// форма имеет площадь. У двух ПАРАЛЛЕЛЬНЫХ отрезков её нет ни у одной, и два коллинеарных отрезка,
// разнесённых вдоль общей прямой, ни одной нормалью не разделяются — SAT объявил бы их
// пересекающимися. Ось вдоль отрезка закрывает ровно этот пробел.
bool separated_along(const WorldShape& a, const WorldShape& b, Vec2 axis) {
    // Из двух зазоров (b правее a и a правее b) положительным может быть только один, поэтому
    // берётся МАКСИМУМ: минимум всегда отрицателен, и проверка через него не отбила бы ничего.
    const fix32 gap = max_fix(support(b, axis, false) - support(a, axis, true),
                              support(a, axis, false) - support(b, axis, true));
    return a.radius + b.radius < gap;
}

bool segments_apart(const WorldShape& a, const WorldShape& b) {
    Vec2 axis;
    if (normalize(a.points[1] - a.points[0], axis).raw != 0 && separated_along(a, b, axis)) {
        return true;
    }
    return normalize(b.points[1] - b.points[0], axis).raw != 0 && separated_along(a, b, axis);
}

} // namespace

bool collide(const Body& a, const Body& b, Manifold& out) {
    // Разворот формы в мировые оси делается здесь, по паре, а не один раз по телу. Цена названа:
    // тело, попавшее в N пар, разворачивается N раз. Кеш на тело — это ещё 264 байта на тело в
    // горячем проходе, и выбирать между ними надо замером (вертикаль 3), а не на глаз.
    WorldShape wa;
    WorldShape wb;
    to_world(a.shape, a.position, a.rot, wa);
    to_world(b.shape, b.position, b.rot, wb);
    out.count = 0;

    if (wa.count == 1) return collide_point_core(wa, a.position, wb, b.position, true, out);
    if (wb.count == 1) return collide_point_core(wb, b.position, wa, a.position, false, out);
    if (wa.count == 2 && wb.count == 2 && segments_apart(wa, wb)) return false;
    return collide_sat(wa, a.position, wb, b.position, out);
}

} // namespace framework::physics
