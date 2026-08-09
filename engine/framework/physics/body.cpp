#include "body.hpp"

namespace framework::physics {

// Приведение к [0, 1) обороту — маской по младшим шестнадцати битам, а не остатком от деления:
// период в оборотах равен единице, то есть ровно 0x10000 в сыром представлении, и приведение
// получается ТОЧНЫМ. Ради этого угол и меряется в оборотах (обоснование целиком — `fixtrig.hpp`).
void set_angle(Body& b, fix32 turns) {
    b.angle = fix32::from_raw(static_cast<int32_t>(static_cast<uint32_t>(turns.raw) & 0xffffu));
    b.rot = rotation(b.angle);
}

Body make_body(const BodyDesc& d) {
    Body b;
    b.key = d.key;
    b.type = d.type;
    // Законность формы наводится здесь и один раз: кламп координат, выпуклая оболочка, нормали.
    // Проверять её в узкой фазе значило бы платить за проверку каждый кадр и всё равно не иметь
    // права ничего исправить — форма к тому моменту уже принадлежит телу.
    b.shape = sanitize(d.shape);
    set_angle(b, d.angle);

    // Тело вращается вокруг центра масс — значит его позиция ЕСТЬ центр масс, а не та точка, от
    // которой автор отсчитывал вершины. Форма поэтому не сдвигается к нулю (она осталась бы не
    // там, где её нарисовали), а переносит смещение на позицию: геометрия остаётся ровно на месте,
    // и при этом у решателя не появляется отдельного случая «начало координат не в центре масс».
    const Vec2 offset = centroid(b.shape);
    for (uint8_t i = 0; i < b.shape.count; ++i) b.shape.points[i] = b.shape.points[i] - offset;
    // Приведение прогоняется ВТОРОЙ раз, потому что сдвиг — перенос, и он вправе вынести вершину за
    // потолок полуразмера: у иглообразного треугольника центроид отстоит от дальней вершины на треть
    // её координаты, и законные 4096 после сдвига становятся 5461. Кламп координаты есть правка
    // формы, а после правки заново нужны и оболочка, и нормали, — поэтому именно `sanitize`, а не
    // отдельный кламп на месте. Для формы, которая в потолок и так укладывается, второй прогон
    // тождественен: обход по монотонной цепи даёт канонический порядок, то есть идемпотентен.
    // Цена — форма, ОБРЕЗАННАЯ здесь, получает центр вращения чуть в стороне от истинного центроида.
    // Это тот же выбор, что и везде в `sanitize`: вход приводится, а не отвергается.
    b.shape = sanitize(b.shape);
    const Vec2 shifted = d.position + rotate(b.rot, offset);
    b.position = {clamp_fix(shifted.x, -WORLD_HALF, WORLD_HALF),
                  clamp_fix(shifted.y, -WORLD_HALF, WORLD_HALF)};

    b.velocity = clamp_speed(d.velocity, MAX_SPEED);
    b.max_angular = max_angular_speed(reach(b.shape));
    b.angular_velocity = clamp_fix(d.angular_velocity, -b.max_angular, b.max_angular);
    b.linear_damping = clamp_fix(d.linear_damping, fix32{}, MAX_DAMPING);
    b.angular_damping = clamp_fix(d.angular_damping, fix32{}, MAX_DAMPING);
    b.material.restitution = clamp_fix(d.material.restitution, fix32{}, MAX_RESTITUTION);
    b.material.friction = clamp_fix(d.material.friction, fix32{}, MAX_FRICTION);
    b.layer = d.layer;
    b.mask = d.mask;
    b.trigger = d.trigger;

    // Статика и кинематика неотличимы для решателя: обе имеют нулевую обратную массу, то есть не
    // получают импульса. Разница между ними — в интеграции, а не здесь.
    if (d.type == BodyType::Dynamic) {
        b.inv_mass = fix32::from_int(1) / clamp_fix(d.mass, MIN_MASS, MAX_MASS);
        // Момент кладётся ПРЯМЫМ и в int64: он у большой формы втрое за потолок Q16.16, а обратный —
        // наоборот, ниже младшего разряда (обоснование целиком у поля `Body::unit_inertia`). Ноль
        // здесь — форма без площади; вращение тогда заперто, и это правда о ней, а не аварийная ветка.
        b.unit_inertia = unit_inertia_raw(b.shape);
    }
    return b;
}

Aabb bounds(const Body& b) { return bounds(b.shape, b.position, b.rot); }

} // namespace framework::physics
