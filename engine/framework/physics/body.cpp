#include "body.hpp"

namespace framework::physics {

Body make_body(const BodyDesc& d) {
    Body b;
    b.key = d.key;
    b.type = d.type;
    // Форма клампится по той же причине, что и масса, и это не перестраховка: полуразмер, при
    // котором AABB выходит за Q16.16, даёт не «очень большое тело», а НЕВЕРНЫЕ границы —
    // насыщенные, то есть одинаковые у всех таких тел. Отрицательный полуразмер даёт вывернутый
    // AABB, для которого `overlaps` ложь всегда: тело без коллизии, о чём игра не узнает.
    b.shape = d.shape;
    b.shape.radius = clamp_fix(d.shape.radius, fix32{}, MAX_SHAPE_HALF);
    b.shape.half = {clamp_fix(d.shape.half.x, fix32{}, MAX_SHAPE_HALF),
                    clamp_fix(d.shape.half.y, fix32{}, MAX_SHAPE_HALF)};
    b.position = {clamp_fix(d.position.x, -WORLD_HALF, WORLD_HALF),
                  clamp_fix(d.position.y, -WORLD_HALF, WORLD_HALF)};
    b.velocity = clamp_speed(d.velocity, MAX_SPEED);
    b.material.restitution = clamp_fix(d.material.restitution, fix32{}, MAX_RESTITUTION);
    b.material.friction = clamp_fix(d.material.friction, fix32{}, MAX_FRICTION);
    // Статика и кинематика неотличимы для решателя: обе имеют нулевую обратную массу, то есть не
    // получают импульса. Разница между ними — в интеграции, а не здесь.
    if (d.type == BodyType::Dynamic) {
        b.inv_mass = fix32::from_int(1) / clamp_fix(d.mass, MIN_MASS, MAX_MASS);
    }
    return b;
}

Aabb bounds(const Body& b) {
    const Vec2 h = b.shape.kind == ShapeKind::Circle ? Vec2{b.shape.radius, b.shape.radius}
                                                     : b.shape.half;
    return {{b.position.x - h.x, b.position.y - h.y}, {b.position.x + h.x, b.position.y + h.y}};
}

bool overlaps(const Aabb& a, const Aabb& b) {
    // Касание границы контактом НЕ считается: `<` вместо `<=`. Иначе два тела, стоящие ровно
    // впритык, каждый кадр рождают контакт с нулевым проникновением, решатель гоняет по нему
    // нулевые импульсы, а стопка «дышит» на величину округления — и это попадает в sim-хеш.
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

} // namespace framework::physics
