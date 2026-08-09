#include "cast.hpp"

#include "distance.hpp"
#include "narrowphase.hpp"

namespace framework::physics {
namespace {

void translate(const WorldShape& s, Vec2 d, WorldShape& out) {
    out.radius = s.radius;
    out.count = s.count;
    for (uint8_t i = 0; i < s.count; ++i) {
        out.points[i] = s.points[i] + d;
        // Нормали переносом НЕ меняются: перенос не вращает форму. Копируются, а не пересчитываются,
        // именно поэтому — пересчёт стоил бы корня на грань на каждой итерации продвижения.
        out.normals[i] = s.normals[i];
    }
}

// Касание в начале пути — отдельный вопрос, и отвечает на него та же узкая фаза, что и шаг мира.
// Своей проверкой здесь обойтись нельзя: перебор вершин (`core_distance`) видит пересечение только
// когда вершина одной формы залезла внутрь другой, а два скрещенных прямоугольника («плюс») дают
// пересечение БЕЗ единой вершины внутри — все вершины снаружи, расстояние положительное. Свип на
// такой раскладке считал бы себя свободным, стоя внутри стены.
bool overlapped_at_start(const WorldShape& moving, Vec2 center, const WorldShape& target,
                         Vec2 target_center, CastHit& out) {
    Manifold m;
    if (!collide_shapes(moving, center, target, target_center, m)) return false;
    out.fraction = fix32{};
    // Нормаль манифольда идёт из `moving` в `target`; наружу из препятствия — обратная. Точка берётся
    // первая: у пересечения в начале пути «где именно коснулись» смысла уже не имеет, а какая-то
    // определённая точка вызывающему нужна.
    out.normal = -m.normal;
    out.point = center + m.points[0].anchor_a;
    return true;
}

} // namespace

bool cast_shape(const WorldShape& moving, Vec2 center, Vec2 travel, const WorldShape& target,
                Vec2 target_center, CastHit& out) {
    if (overlapped_at_start(moving, center, target, target_center, out)) return true;

    WorldShape moved;
    fix32 t;
    for (uint32_t iter = 0;; ++iter) {
        translate(moving, travel * t, moved);
        // Предусловие `core_distance` — ядра не пересекаются — здесь держится не проверкой, а
        // ПОСТРОЕНИЕМ: в начале пути пересечение отбито узкой фазой выше, а продвижение по
        // определению останавливается ДО касания и потому внутрь не заходит ни на одной итерации.
        const CoreDistance d = core_distance(moved, target);
        const fix32 separation = d.dist - moved.radius - target.radius;

        // Исчерпанный потолок итераций отвечает КАСАНИЕМ на достигнутой доле, а не «путь свободен»,
        // и выбор здесь не симметричен: «свободен» от выдохшегося свипа означает персонажа,
        // въехавшего в стену, а лишняя остановка перед препятствием — чуть более раннее торможение,
        // которого игрок не отличит от честного касания.
        if (!(CONTACT_SLOP < separation) || iter + 1 >= MAX_CAST_ITERATIONS) {
            out.fraction = t;
            out.normal = -d.dir;
            // `d.on_a` посчитана уже на ПЕРЕНЕСЁННОЙ форме, то есть в мировых осях на доле `t`;
            // прибавлять смещение второй раз было бы двойным переносом.
            out.point = d.on_a + d.dir * moved.radius;
            return true;
        }

        // Сближение считается вдоль направления РАЗДЕЛЕНИЯ и за весь путь целиком, поэтому частное
        // сразу выражено в долях пути. Неположительное сближение — не «пока не сближаются»: `d.dir`
        // задаёт опорную плоскость, за которую формы не заходят, и путь, не приближающий к ней,
        // не приблизит к ней и дальше. Это доказательство, а не эвристика.
        const fix32 closing = dot(travel, d.dir);
        if (!(closing.raw > 0)) return false;

        // Насыщение частного здесь БЕЗОПАСНО и означает ровно то, что нужно: разделение больше, чем
        // всё сближение за путь, — значит касания на пути нет. Улетевшее за единицу `t` отсекается
        // следующей строкой, а не молча продолжает перебор.
        t = t + separation / closing;
        if (fix32::from_int(1) < t) return false;
    }
}

} // namespace framework::physics
