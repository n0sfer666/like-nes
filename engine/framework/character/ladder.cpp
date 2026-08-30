#include "ladder.hpp"

namespace framework::character {
namespace {

using tilemap::TileGrid;

// Есть ли лестница В ТОЧКЕ. Точечный зонд, а не `overlap_shape`: тот держит `std::vector`, а тик
// персонажа обязан остаться zero-alloc (инвариант 5 спеки #16, пиннут счётчиком обращений к куче).
// Окно вырожденной коробки уже отсечено картой, поэтому точка за краем отвечает «нет» сама.
bool ladder_at(const TileGrid& g, Vec2 point) {
    const tilemap::TileWindow w = g.window(physics::Aabb{point, point});
    if (w.empty()) return false;
    return (g.at(w.x0, w.y0) & tilemap::TILE_LADDER) != 0;
}

// Зонд ПОД НОГАМИ. Без него невыразимы оба конца шахты: стоящий на верхней площадке персонаж центром
// находится в тайле НАД лестницей, то есть в пустоте, а вылезающий наверх теряет её центром тогда,
// когда ноги ещё внутри площадки, — и отпущенный там проваливался бы обратно в шахту. Отступ вниз —
// четверть тайла: ровно ноги дали бы ответ, зависящий от зазора контакта (персонаж стоит на волосок
// выше грани), а половина тайла у низкой формы промахнулась бы мимо площадки насквозь.
Vec2 below_feet(const TileGrid& g, const CharacterHull& hull, Vec2 position) {
    const physics::Aabb box = physics::bounds(hull.shape, position, Rot{});
    return {position.x, box.max.y + fix32::from_raw(g.tile_size().raw / 4)};
}

// Середина колонны. Размер тайла приведён к ЧЁТНОМУ числу raw (`grid.cpp`), поэтому половина точна
// и делить `fix32` не приходится.
fix32 column_center(const TileGrid& g, Vec2 point) {
    const tilemap::TileWindow w = g.window(physics::Aabb{point, point});
    if (w.empty()) return point.x;
    return g.tile_bounds(w.x0, w.y0).min.x + fix32::from_raw(g.tile_size().raw / 2);
}

// Сцена тика лестницы: односторонние тайлы сняты со ВСЕХ запросов разом. Иначе верхняя площадка
// (`solid oneway ladder`) держит спускающегося собственным правилом «пришёл сверху» — ноги на её
// грани, — и спуск по лестнице кончался бы на первом же тике, не начавшись.
CollisionScene shaft(const CollisionScene& s) {
    CollisionScene out = s;
    out.tiles.exclude = static_cast<tilemap::TileFlags>(out.tiles.exclude | tilemap::TILE_ONEWAY);
    return out;
}

// Схватиться. Намерение ОБЯЗАТЕЛЬНО: лестница, хватающая проходящего мимо, отменяет прыжок через
// шахту, а он в платформере — обычное движение, а не редкий случай.
bool grab(const CollisionScene& s, const CharacterHull& hull, const MoveProfile& p,
          const MoveInput& in, Character& c) {
    if (s.grid == nullptr || p.climb_speed.raw == 0) return false;
    if (!in.up_held && !in.down_held) return false;
    const TileGrid& g = *s.grid;
    // «Вверх» хватает откуда угодно — с земли, из полёта, с прыжка; «вниз» только СО СТОЯНИЯ, и
    // разница не в симметрии, а в том, что падающий сквозь шахту с зажатым «вниз» иначе прилипал бы
    // к ней ровно там, куда не целился.
    // «Вверх» спрашивает ЦЕНТР, а не ноги: стоящий на верхней площадке иначе хватался бы за неё же
    // и лез в пустоту над шахтой. Удержание там — обычное «иду вверх», а не команда лезть.
    const bool up = in.up_held && ladder_at(g, c.position);
    const bool down = !up && in.down_held && c.on_ground && ladder_at(g, below_feet(g, hull, c.position));
    if (!up && !down) return false;

    c.state = MoveState::Ladder;
    c.on_ground = false;
    c.velocity = Vec2{};
    c.jump_active = false;
    // Запрос прыжка ИЗРАСХОДОВАН вместе с окнами: буфер, доживший до лестницы, выдал бы прыжок с
    // неё тем же тиком, в котором за неё схватились, а окно coyote разрешило бы его в воздухе.
    c.buffer_left = 0;
    c.coyote_left = 0;
    c.support = physics::BodyId{physics::BodyId::INVALID};

    // Выравнивание по колонне идёт СВИПОМ, а не присваиванием: персонаж, схватившийся у самого края
    // тайла, иначе телепортировался бы на полтайла вбок — в том числе внутрь стены, стоящей вплотную
    // к шахте. Не поместилось — сдвигаемся насколько влезло, и это законный исход, а не отказ.
    const fix32 dx = column_center(g, c.position) - c.position.x;
    SceneHit h;
    if (cast_nearest(s, hull, c.position, {dx, fix32{}}, h)) c.position.x = c.position.x + dx * h.fraction;
    else c.position.x = c.position.x + dx;
    return true;
}

} // namespace

bool ladder_step(const CollisionScene& s, const CharacterHull& hull, const MoveProfile& p,
                 const MoveDerived& d, const MoveInput& in, fix32 dt, Character& c) {
    // Окно ЧИТАЕТСЯ до истечения и истекает после, ровно как окна прощения в шаге 7 контроллера.
    // Обратный порядок укоротил бы его на тик: восьмой тик после прыжка уже хватал бы лестницу, то
    // есть «восемь тиков» означало бы семь.
    const bool locked = c.ladder_regrab_left > 0;
    if (locked) --c.ladder_regrab_left;
    if (c.state != MoveState::Ladder && (locked || !grab(s, hull, p, in, c))) return false;

    // Прыжок с лестницы. Фронт нажатия выводится здесь по той же памяти, что и в контроллере, —
    // `jump_was_held` пишется в конце КАЖДОГО тика, чей бы он ни был, и второй точки правды у него
    // нет. Горизонталь при этом ноль: разгон считает шаг 2 обычного тика со следующего тика, и
    // выдать её здесь целиком значило бы отменить `air_accel` ровно там, где игрок целится.
    const bool jumped = in.jump_held && !c.jump_was_held;
    if (jumped) {
        c.velocity = {fix32{}, -d.jump_speed};
        c.jump_active = true;
        // Окно перехвата — единственное, что делает прыжок с лестницы наблюдаемым: без него зонд
        // «вверх» находит ту же лестницу тем же тиком, и персонаж возвращается на неё, не успев с
        // неё уйти.
        c.ladder_regrab_left = p.ladder_regrab_ticks;
        c.state = MoveState::Air;
    } else {
        // Лазание. Скорость ПОСТОЯННА и одна на обе стороны: разгон по вертикали дал бы лестницу,
        // по которой сползают тем быстрее, чем дольше держат кнопку, — то есть падение с задержкой.
        c.velocity.x = fix32{};
        c.velocity.y = in.up_held ? -p.climb_speed : (in.down_held ? p.climb_speed : fix32{});
    }

    const CollisionScene tick = shaft(s);
    const SlideResult sr = move_and_slide(tick, hull, c.velocity * dt, p.max_slope, c.position,
                                          c.velocity);
    c.position.x = clamp_fix(c.position.x, -physics::WORLD_HALF, physics::WORLD_HALF);
    c.position.y = clamp_fix(c.position.y, -physics::WORLD_HALF, physics::WORLD_HALF);
    c.hit_ceiling = sr.hit_ceiling;
    c.hit_wall = sr.hit_wall;
    c.crushed = false;

    if (c.state == MoveState::Ladder) {
        // Лестница кончилась — верх шахты пройден центром. Отпускать в ВОЗДУХ, а не ставить на
        // площадку: обычный тик уронит персонажа на неё тяготением и найдёт опору своей пробой, а
        // назначить её здесь значило бы завести второе место, где решается «стою ли я».
        const bool touching = s.grid != nullptr && (ladder_at(*s.grid, c.position) ||
                                                    ladder_at(*s.grid, below_feet(*s.grid, hull, c.position)));
        if (!touching) c.state = MoveState::Air;
        // Низ шахты: под ногами пол. «Вверх» этот выход не закрывает — иначе схватившийся с земли
        // сходил бы с лестницы тем же тиком, в котором за неё взялся.
        else if (!in.up_held && probe_ground(tick, hull, c.position, p.max_slope, nullptr)) {
            c.state = MoveState::Ground;
            c.on_ground = true;
            c.velocity = Vec2{};
        }
    }
    c.jump_was_held = in.jump_held;
    // Тик съеден ВСЕГДА, если досюда дошли: движение уже случилось, и вернуть здесь `false` значило
    // бы посчитать его второй раз обычным шагом — то есть удвоить скорость на тике схода.
    return true;
}

} // namespace framework::character
