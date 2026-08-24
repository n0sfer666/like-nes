#include "query.hpp"

#include <algorithm>

#include "cast.hpp"
#include "narrowphase.hpp"
#include "units.hpp"

namespace framework::tilemap {
namespace {

using physics::Aabb;

bool participates(TileFlags flags, const TileFilter& f) { return (flags & f.require) != 0; }

// Коробка, покрывающая ВЕСЬ путь свипа. Тот же довод, что в `physics/query.cpp`: тайл, лежащий в
// конце пути, в стартовую коробку не попадает, и запрос молча отвечал бы «свободно» ровно там, где
// летит пуля.
Aabb swept(const Aabb& a, Vec2 travel) {
    const Aabb b{a.min + travel, a.max + travel};
    return {{min_fix(a.min.x, b.min.x), min_fix(a.min.y, b.min.y)},
            {max_fix(a.max.x, b.max.x), max_fix(a.max.y, b.max.y)}};
}

// Внутренних граней запрос НЕ отсекает, и это решение по замеру, а не по недосмотру.
//
// Сетка из отдельных коробок вроде бы обязана отдавать зацеп за стык: персонаж, идущий по плоскому
// полу, упирается в боковую грань следующего тайла и встаёт на ровном месте. Отсечение таких граней
// («за гранью стоит такой же тайл — значит грани нет») здесь было написано и снято обратно, потому
// что воспроизвести зацеп не удалось НИ ОДНОЙ сценой: 9773 случайных свипа со свободного старта и
// 490 сцен с выровненными координатами (зазор от −1/8 до +1/64, смещение по X от 0 до 8.5, длины
// 1/3/5/16/17/48/64, оба направления) дали ноль расхождений между «с отсечением» и «без». Ноль
// вышел и на ДО-фиксовом правиле нормали (`terminal_normal`, коммит 5ecda91), то есть зацеп не
// прятался за той ничьей тоже.
//
// Причину видно в замере по зазорам: свип вдоль плоского пола либо не видит его вовсе (тело дальше
// `CONTACT_SLOP` — ответ «свободно»), либо УЖЕ в контакте и получает долю ноль с верхней нормалью.
// Боковая грань соседа не выигрывает никогда, потому что путь до неё проходит через самого соседа,
// а сосед участвует в том же запросе и отвечает своей гранью не позже. Это геометрия, и держит
// инвариант 3 спеки она, а не код.
//
// Цена отсечения при этом была не нулевой: тело, оказавшееся ЦЕЛИКОМ внутри тайлов (спавн в стене,
// телепорт), теряло ВСЕ грани разом и получало «путь свободен» — 2360 из 20000 случайных свипов.
// Теперь оно получает долю ноль, ровно как от `physics::World` (решение владельца 2026-08-23):
// контракт у сетки и у мира один, и контроллеру не нужно знать, кто ему ответил.

// Ближайшее по доле пути, ничьи разводятся ИНДЕКСОМ тайла. Без разведения ответ зависел бы от
// порядка обхода окна, то есть от того, где стоит зонд, — а угол в угол на осевой сетке это не
// редкий случай, а норма жизни тайлмапа.
// Осевая грань бьёт диагональ, и только потом решает индекс. На осевой сетке всякая НАСТОЯЩАЯ
// грань даёт осевую нормаль; диагональ означает касание УГЛОМ В УГОЛ, где входы по обеим осям
// совпали до кванта и ось контакта не определена вовсе — узкая фаза отдаёт там обратное
// направление пути. Без этого правила порядок решал индекс, то есть номер тайла, и персонаж,
// садящийся на ПЛОСКИЙ пол ровно на стыке двух тайлов, получал вместо нормали пола диагональ:
// скольжение вдоль пола обнулялось, и он вставал на ровном месте (найдено переездом контроллера
// на сетку, гейт угла в `framework_character_tunnel_test`).
bool axial(Vec2 n) { return n.x == fix32{} || n.y == fix32{}; }

bool closer(const TileHit& candidate, const TileHit& best, bool have_best) {
    if (!have_best) return true;
    if (!(candidate.fraction == best.fraction)) return candidate.fraction < best.fraction;
    if (axial(candidate.normal) != axial(best.normal)) return axial(candidate.normal);
    return candidate.index < best.index;
}

// Единственный вход всех трёх запросов. Общий он не ради краткости: счётчик цены обязан считать у
// всех троих одно и то же, иначе гейт инварианта 4 отвечает про тот запрос, который переписали
// последним, а про остальные молчит.
template <class Visit>
void scan_window(const TileGrid& g, const Aabb& raw_probe, const TileFilter& f, Visit&& visit) {
    // Поле у коробки зонда — `2 * CONTACT_SLOP`, и оно ЦЕЛИКОМ на зонде: тайлы расширять нечем, их
    // коробки считаются из раскладки. Запас тот же, что у полосы кандидатов физики, и по той же
    // причине: свип принимает касание вплоть до допуска ВКЛЮЧИТЕЛЬНО, а окно отсекает строго, —
    // односторонний запас уже был дефектом перф-раунда, когда зажатый впритык контроллер переставал
    // видеть стену.
    const fix32 pad = physics::CONTACT_SLOP + physics::CONTACT_SLOP;
    const Aabb probe{{raw_probe.min.x - pad, raw_probe.min.y - pad},
                     {raw_probe.max.x + pad, raw_probe.max.y + pad}};
    const TileWindow win = g.window(probe);
    const fix32 half = fix32::from_raw(g.tile_size().raw / 2);
    const physics::Shape core = physics::sanitize(physics::box(half, half));
    const Rot no_rot = rotation(fix32{});
    uint64_t scanned = 0;
    for (int32_t y = win.y0; y < win.y1; ++y) {
        for (int32_t x = win.x0; x < win.x1; ++x) {
            // Счётчик считает КАЖДЫЙ тайл окна, в том числе пустой: мера здесь — размер окна, а не
            // плотность карты. Считать после проверки флагов значило бы получать ноль на пустом
            // уровне и объявлять это независимостью от размера.
            ++scanned;
            if (!participates(g.at(x, y), f)) continue;
            const Aabb b = g.tile_bounds(x, y);
            const Vec2 centre{b.min.x + half, b.min.y + half};
            physics::WorldShape target;
            to_world(core, centre, no_rot, target);
            visit(x, y, centre, target);
        }
    }
    detail::TileQuerySeam::note(g, scanned);
}

uint32_t tile_index(const TileGrid& g, int32_t x, int32_t y) {
    return static_cast<uint32_t>(y) * g.width() + static_cast<uint32_t>(x);
}

// Форма запроса приводится тем же `sanitize`, что и форма тела: невыпуклый многоугольник даёт SAT
// не грубый ответ, а НЕВЕРНЫЙ — с нормалью внутрь.
physics::Shape query_core(const physics::Shape& s) { return physics::sanitize(s); }

// Луч собирается вручную, минуя `sanitize`: тот назначает ядру без площади принудительный радиус, и
// луч с радиусом перестаёт быть лучом — он цепляет углы мимо линии выстрела (`physics/query.cpp`).
void ray_shape(Vec2 origin, physics::WorldShape& out) {
    out = physics::WorldShape{};
    out.count = 1;
    out.points[0] = origin;
}

bool cast_against_grid(const TileGrid& g, const physics::WorldShape& moving, Vec2 center,
                       Vec2 travel, const Aabb& probe, const TileFilter& f, TileHit& out) {
    bool found = false;
    scan_window(g, swept(probe, travel), f,
                [&](int32_t x, int32_t y, Vec2 centre, const physics::WorldShape& target) {
                    physics::CastHit hit;
                    if (!cast_shape(moving, center, travel, target, centre, hit)) return;
                    const TileHit candidate{x,          y,         tile_index(g, x, y),
                                            hit.fraction, hit.point, hit.normal};
                    if (!closer(candidate, out, found)) return;
                    out = candidate;
                    found = true;
                });
    return found;
}

} // namespace

void overlap_shape(const TileGrid& g, const physics::Shape& s, Vec2 position, fix32 angle,
                   const TileFilter& f, std::vector<TileOverlap>& out) {
    out.clear();
    const physics::Shape core = query_core(s);
    const Rot rot = rotation(angle);
    physics::WorldShape probe;
    to_world(core, position, rot, probe);

    scan_window(g, physics::bounds(core, position, rot), f,
                [&](int32_t x, int32_t y, Vec2 centre, const physics::WorldShape& target) {
                    physics::Manifold m;
                    // Той же узкой фазой, что и шаг мира, и с НУЛЕВЫМ спекулятивным полем: запрос
                    // отвечает на «кто здесь СЕЙЧАС», и тайл в 1/16 юнита от области в ответе быть
                    // не должен. Своя проверка пересечения была бы вторым ответом на тот же вопрос.
                    if (!collide_shapes(probe, position, target, centre, fix32{}, m)) return;
                    out.push_back({x, y, tile_index(g, x, y)});
                });
    std::sort(out.begin(), out.end(),
              [](const TileOverlap& l, const TileOverlap& r) { return l.index < r.index; });
}

bool raycast(const TileGrid& g, Vec2 origin, Vec2 delta, const TileFilter& f, TileHit& out) {
    physics::WorldShape ray;
    ray_shape(origin, ray);
    return cast_against_grid(g, ray, origin, delta, {origin, origin}, f, out);
}

bool shapecast(const TileGrid& g, const physics::Shape& s, Vec2 position, fix32 angle, Vec2 travel,
               const TileFilter& f, TileHit& out) {
    const physics::Shape core = query_core(s);
    const Rot rot = rotation(angle);
    physics::WorldShape moving;
    to_world(core, position, rot, moving);
    return cast_against_grid(g, moving, position, travel, physics::bounds(core, position, rot), f,
                             out);
}

} // namespace framework::tilemap
