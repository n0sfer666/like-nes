#include "query.hpp"

#include <algorithm>

#include "cast.hpp"
#include "narrowphase.hpp"
#include "tile_rules.hpp"
#include "tile_shape.hpp"
#include "units.hpp"

namespace framework::tilemap {
namespace {

using physics::Aabb;

using detail::closer;
using detail::lowest_y;
using detail::oneway_holds;
using detail::participates;

// Что запрос узнал о тайле, дойдя до геометрии. Структурой, а не пятью позиционными параметрами:
// обходчик один на три запроса, и порядок из шести аргументов — это ошибка, которую компилятор не
// заметит, потому что координаты, флаги и размеры у него одного типа.
struct Scanned {
    int32_t x;
    int32_t y;
    TileFlags flags;
    Aabb bounds;
    Vec2 centre;
};

// Коробка, покрывающая ВЕСЬ путь свипа. Тот же довод, что в `physics/query.cpp`: тайл, лежащий в
// конце пути, в стартовую коробку не попадает, и запрос молча отвечал бы «свободно» ровно там, где
// летит пуля.
Aabb swept(const Aabb& a, Vec2 travel) {
    const Aabb b{a.min + travel, a.max + travel};
    return {{min_fix(a.min.x, b.min.x), min_fix(a.min.y, b.min.y)},
            {max_fix(a.max.x, b.max.x), max_fix(a.max.y, b.max.y)}};
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
    TileShapes shapes(half);
    const Rot no_rot = rotation(fix32{});
    uint64_t scanned = 0;
    for (int32_t y = win.y0; y < win.y1; ++y) {
        for (int32_t x = win.x0; x < win.x1; ++x) {
            // Счётчик считает КАЖДЫЙ тайл окна, в том числе пустой: мера здесь — размер окна, а не
            // плотность карты. Считать после проверки флагов значило бы получать ноль на пустом
            // уровне и объявлять это независимостью от размера.
            ++scanned;
            const TileFlags flags = g.at(x, y);
            if (!participates(flags, f)) continue;
            const Aabb b = g.tile_bounds(x, y);
            const Vec2 centre{b.min.x + half, b.min.y + half};
            physics::WorldShape target;
            to_world(shapes.of(flags), centre, no_rot, target);
            visit(Scanned{x, y, flags, b, centre}, target);
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
    const fix32 bottom = lowest_y(moving);
    scan_window(g, swept(probe, travel), f, [&](const Scanned& t, const physics::WorldShape& target) {
        physics::CastHit hit;
        if (!cast_shape(moving, center, travel, target, t.centre, hit)) return;
        if ((t.flags & TILE_ONEWAY) != 0 && !oneway_holds(hit.normal, t.bounds, bottom)) return;
        const TileHit candidate{t.x,          t.y,       tile_index(g, t.x, t.y),
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
                [&](const Scanned& t, const physics::WorldShape& target) {
                    physics::Manifold m;
                    // Той же узкой фазой, что и шаг мира, и с НУЛЕВЫМ спекулятивным полем: запрос
                    // отвечает на «кто здесь СЕЙЧАС», и тайл в 1/16 юнита от области в ответе быть
                    // не должен. Своя проверка пересечения была бы вторым ответом на тот же вопрос.
                    //
                    // Правило одностороннего тайла сюда не приходит: у перекрытия нет пути, а
                    // значит нет и того, откуда пришли. Платформа, накрывшая область, в ответе есть.
                    if (!collide_shapes(probe, position, target, t.centre, fix32{}, m)) return;
                    out.push_back({t.x, t.y, tile_index(g, t.x, t.y)});
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
