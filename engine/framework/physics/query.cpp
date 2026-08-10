#include "query.hpp"

#include <algorithm>

#include "narrowphase.hpp"

namespace framework::physics {
namespace {

bool accepts(const QueryFilter& f, const Body& b) {
    if (b.trigger && !f.include_triggers) return false;
    return query_accepts(f.mask, b.layer);
}

// Форма запроса приводится тем же `sanitize`, что и форма тела. Не перестраховка: невыпуклый или
// перечисленный по часовой многоугольник даёт SAT не грубый ответ, а НЕВЕРНЫЙ — с нормалью внутрь.
// Запрос, у которого нормаль смотрит внутрь препятствия, отправит персонажа сквозь стену ровно так
// же, как это сделал бы контакт.
void query_shape(const Shape& s, Vec2 position, fix32 angle, WorldShape& out) {
    to_world(sanitize(s), position, rotation(angle), out);
}

// Луч собирается ВРУЧНУЮ, минуя `sanitize`, и это единственное место модуля, где так можно. Причина
// названа прямо: `sanitize` назначает ядру без площади принудительный радиус (`MIN_SHAPE_EXTENT`) —
// для тела это правильно, иначе оно ни с чем не сталкивается, — а луч, получивший радиус, перестаёт
// быть лучом и становится тонкой капсулой, которая цепляет углы мимо линии выстрела.
void ray_shape(Vec2 origin, WorldShape& out) {
    out = WorldShape{};
    out.count = 1;
    out.points[0] = origin;
}

// Ближайшее по доле пути, ничьи разводятся КЛЮЧОМ. Без разведения два тела, задетые на одной доле
// (угол в угол на осевой сетке — обычное дело), отвечали бы в порядке индексов, то есть создания, —
// и гейт 2, перетасовывающий создание, поймал бы расхождение запроса с самим собой.
bool closer(const RayHit& candidate, const RayHit& best, bool have_best) {
    if (!have_best) return true;
    if (!(candidate.fraction == best.fraction)) return candidate.fraction < best.fraction;
    return candidate.key < best.key;
}

bool cast_against_world(const World& w, const WorldShape& moving, Vec2 center, Vec2 travel,
                        const QueryFilter& f, RayHit& out) {
    bool found = false;
    const std::vector<Body>& bodies = w.bodies();
    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies.size()); ++i) {
        const Body& b = bodies[i];
        if (!accepts(f, b)) continue;
        WorldShape target;
        to_world(b.shape, b.position, b.rot, target);
        CastHit hit;
        if (!cast_shape(moving, center, travel, target, b.position, hit)) continue;
        const RayHit candidate{BodyId{i}, b.key, hit.fraction, hit.point, hit.normal};
        if (!closer(candidate, out, found)) continue;
        out = candidate;
        found = true;
    }
    return found;
}

} // namespace

void overlap_shape(const World& w, const Shape& s, Vec2 position, fix32 angle, const QueryFilter& f,
                   std::vector<Overlap>& out) {
    out.clear();
    WorldShape probe;
    query_shape(s, position, angle, probe);

    const std::vector<Body>& bodies = w.bodies();
    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies.size()); ++i) {
        const Body& b = bodies[i];
        if (!accepts(f, b)) continue;
        WorldShape target;
        to_world(b.shape, b.position, b.rot, target);
        Manifold m;
        // Той же узкой фазой, что и шаг мира, но с НУЛЕВЫМ спекулятивным полем. Своя проверка
        // пересечения здесь была бы вторым ответом на тот же вопрос, и разошлись бы они на касании.
        // Поле при этом принадлежит шагу, а не геометрии: шаг смотрит вперёд на кадр и вправе считать
        // контактом почти-касание, а запрос отвечает на «кто здесь СЕЙЧАС» — и тело, стоящее в 1/16
        // юнита от области, в ответе быть не должно.
        if (!collide_shapes(probe, position, target, b.position, fix32{}, m)) continue;
        out.push_back({BodyId{i}, b.key});
    }
    std::sort(out.begin(), out.end(),
              [](const Overlap& l, const Overlap& r) { return l.key < r.key; });
}

bool raycast(const World& w, Vec2 origin, Vec2 delta, const QueryFilter& f, RayHit& out) {
    WorldShape ray;
    ray_shape(origin, ray);
    return cast_against_world(w, ray, origin, delta, f, out);
}

bool shapecast(const World& w, const Shape& s, Vec2 position, fix32 angle, Vec2 travel,
               const QueryFilter& f, RayHit& out) {
    WorldShape moving;
    query_shape(s, position, angle, moving);
    return cast_against_world(w, moving, position, travel, f, out);
}

} // namespace framework::physics
