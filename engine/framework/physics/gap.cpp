#include "gap.hpp"

#include <cstdint>

#include "distance.hpp"

namespace framework::physics {

bool collide_core_gap(const WorldShape& a, Vec2 center_a, const WorldShape& b, Vec2 center_b,
                      Manifold& out) {
    // Перебор вершин живёт в `distance.cpp`: тем же вопросом отвечает свип формы (`cast.cpp`), и
    // вторая его реализация разошлась бы с этой на округлении. Предусловие «ядра не пересекаются»
    // держит вызывающий — обоснование целиком в заголовке.
    const CoreDistance best = core_distance(a, b);

    // Расстояние берётся со знаком, и минимум — тоже: на отрицательной ветке это САМАЯ ГЛУБОКАЯ из
    // найденных точек, а не кратчайший выход. Выбор намеренный и именно в эту сторону: занизить
    // проникновение значит оставить пару перекрытой ещё на кадр, завысить — потратить лишнюю
    // итерацию решателя.
    const fix32 separation = best.dist - a.radius - b.radius;
    if (separation.raw > 0) return false;

    // Точка контакта — СЕРЕДИНА между поверхностями, тем же правилом, что у SAT и у точечного ядра:
    // взять поверхность одной из форм значило бы сместить плечо на половину проникновения в её
    // сторону, и момент зависел бы от того, какая форма оказалась первой в паре.
    const Vec2 point = best.on_a + best.dir * (a.radius + fix32::from_raw(separation.raw / 2));

    out.normal = best.dir;
    out.count = 1;
    ManifoldPoint& p = out.points[0];
    p = ManifoldPoint{};
    p.anchor_a = point - center_a;
    p.anchor_b = point - center_b;
    p.penetration = -separation;
    // Идентификатор собирается здесь, а не в переборе: пространство идентификаторов принадлежит
    // путям узкой фазы (`contact.hpp`), а расстояние — общий геометрический примитив, у которого
    // потребителей двое. Половины перебора разведены байтом: одна и та же вершина, найденная с
    // разных сторон, — это разные геометрические причины, и накопленное с одной не переносится
    // на другую.
    p.id = CORE_GAP_ID_BIT | (best.witness_in_a ? 0x100u : 0u) | static_cast<uint32_t>(best.vertex);
    return true;
}

} // namespace framework::physics
