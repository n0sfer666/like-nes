#include "query.hpp"

#include <algorithm>

#include "narrowphase.hpp"
#include "query_index.hpp"
#include "units.hpp"

namespace framework::physics {
namespace {

bool accepts(const QueryFilter& f, const Body& b) {
    if (b.trigger && !f.include_triggers) return false;
    return query_accepts(f.mask, b.layer);
}

// AABB формы, уже приведённой в мировые оси. Считается здесь, а не через `bounds(Shape, ...)`, по
// той же причине, по которой узкая фаза работает с `WorldShape`: разворот уже сделан, и второй
// разворот ради одной коробки — это не только лишняя тригонометрия, но и второй ответ на тот же
// вопрос, который вправе разойтись с первым на округлении.
//
// ПРЕДУСЛОВИЕ: `s.count >= 1`. Оба входа модуля его держат (`sanitize` оставляет минимум одну
// точку, `ray_shape` ставит ровно одну), но само по себе оно не выполняется: `WorldShape{}` — это
// count == 0 и нулевые точки, то есть полоса ВОКРУГ НАЧАЛА КООРДИНАТ. Такой вход отвечает не
// падением, а тихо неверным набором кандидатов, поэтому предусловие названо, как у `core_distance`.
Aabb world_bounds(const WorldShape& s) {
    Vec2 lo = s.points[0];
    Vec2 hi = s.points[0];
    for (uint8_t i = 1; i < s.count; ++i) {
        if (s.points[i].x < lo.x) lo.x = s.points[i].x;
        if (s.points[i].y < lo.y) lo.y = s.points[i].y;
        if (hi.x < s.points[i].x) hi.x = s.points[i].x;
        if (hi.y < s.points[i].y) hi.y = s.points[i].y;
    }
    const Vec2 r = {s.radius, s.radius};
    return {lo - r, hi + r};
}

// Коробка, покрывающая ВЕСЬ путь свипа: форма на старте и она же, сдвинутая на перемещение. Свип не
// вправе отсекаться по стартовой коробке — тело, стоящее в конце пути, в неё не попадает, и запрос
// молча возвращал бы «ни с кем не пересеклись» ровно там, где летит пуля.
Aabb swept(const Aabb& a, Vec2 travel) {
    Aabb b{a.min + travel, a.max + travel};
    return {{a.min.x < b.min.x ? a.min.x : b.min.x, a.min.y < b.min.y ? a.min.y : b.min.y},
            {b.max.x < a.max.x ? a.max.x : b.max.x, b.max.y < a.max.y ? a.max.y : b.max.y}};
}

// Полоса кандидатов — ЕДИНСТВЕННЫЙ вход всех трёх запросов к телам мира. Общая она не ради
// краткости: счётчик рассмотренных обязан считать у всех троих одно и то же, иначе гейт цены
// отвечает про тот запрос, который переписали последним, а про остальные молчит.
//
// Порядок обхода полосы — по возрастанию левой границы, а не по индексам тел, и оба потребителя к
// этому готовы: перекрытие сортирует ответ ключом, свип разводит ничьи ключом же (`closer`).
// Порядок, зависящий от РАСКЛАДКИ, а не от создания, — здесь не оговорка, а требование гейта 2.
template <class Visit>
void scan_band(const World& w, const Aabb& raw_probe, const QueryFilter& f, Visit&& visit) {
    // Коробка зонда расширяется на спекулятивное поле — ВТОРАЯ половина того же расширения, первую
    // индекс делает над телами (`query_index.cpp`). Симметрия здесь не эстетика, а точная копия
    // широкой фазы, и односторонний вариант уже был дефектом этого раунда: `overlaps` сравнивает
    // СТРОГО, свип же считает касанием расстояние вплоть до `CONTACT_SLOP` включительно, — поэтому
    // при расширении с одной стороны персонаж, стоящий у стены ровно в допуске свипа, переставал
    // видеть эту стену. Наблюдалось это не отказом, а тем, что контроль гейта туннелирования,
    // обязанный стоять зажатым, свободно уезжал на четыре юнита.
    //
    // ГРАНИЦА этого доказательства названа прямо: суммарный запас полосы — ровно `2 * CONTACT_SLOP`,
    // и покрывает он допуск, с которым свип принимает касание. Второго выхода `cast_shape` — отказа
    // по исчерпанию `MAX_CAST_ITERATIONS`, отвечающего касанием при ЛЮБОМ разделении, — он не
    // покрывает и покрыть не может: величины, ограничивающей то разделение сверху, не существует.
    // На вырожденной геометрии, где свип выдыхается дальше 0.125, ответ через индекс поэтому вправе
    // разойтись с линейным обходом — это граница контракта запроса, а не дефект полосы.
    const Vec2 pad = {SPECULATIVE_MARGIN, SPECULATIVE_MARGIN};
    const Aabb probe{raw_probe.min - pad, raw_probe.max + pad};
    const QueryIndex& index = w.query_index();
    const std::vector<Body>& bodies = w.bodies();
    const Band band = index.band(bodies, probe);
    uint64_t scanned = 0;
    for (uint32_t slot = band.begin; slot < band.end; ++slot) {
        ++scanned;
        if (!overlaps(probe, index.bounds_at(slot))) continue;
        const uint32_t i = index.body_at(slot);
        const Body& b = bodies[i];
        if (!accepts(f, b)) continue;
        visit(i, b);
    }
    // Счётчик пишется ВСЕГДА, в том числе на пустой полосе: ноль рассмотренных — тоже ответ, а
    // счётчик, обновляемый только при попаданиях, донёс бы число предыдущего запроса.
    detail::QuerySeam::note(index, scanned);
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
    scan_band(w, swept(world_bounds(moving), travel), f, [&](uint32_t i, const Body& b) {
        WorldShape target;
        to_world(b.shape, b.position, b.rot, target);
        CastHit hit;
        if (!cast_shape(moving, center, travel, target, b.position, hit)) return;
        const RayHit candidate{BodyId{i}, b.key, hit.fraction, hit.point, hit.normal};
        if (!closer(candidate, out, found)) return;
        out = candidate;
        found = true;
    });
    return found;
}

} // namespace

void overlap_shape(const World& w, const Shape& s, Vec2 position, fix32 angle, const QueryFilter& f,
                   std::vector<Overlap>& out) {
    out.clear();
    WorldShape probe;
    query_shape(s, position, angle, probe);

    scan_band(w, world_bounds(probe), f, [&](uint32_t i, const Body& b) {
        WorldShape target;
        to_world(b.shape, b.position, b.rot, target);
        Manifold m;
        // Той же узкой фазой, что и шаг мира, но с НУЛЕВЫМ спекулятивным полем. Своя проверка
        // пересечения здесь была бы вторым ответом на тот же вопрос, и разошлись бы они на касании.
        // Поле при этом принадлежит шагу, а не геометрии: шаг смотрит вперёд на кадр и вправе считать
        // контактом почти-касание, а запрос отвечает на «кто здесь СЕЙЧАС» — и тело, стоящее в 1/16
        // юнита от области, в ответе быть не должно.
        if (!collide_shapes(probe, position, target, b.position, fix32{}, m)) return;
        out.push_back({BodyId{i}, b.key});
    });
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
