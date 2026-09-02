#include "platformer_scene.hpp"

// КАДР уровня: границы, тайл под точкой, край карты и один шаг игры. Чтение бандла живёт отдельным
// файлом (`platformer_level.cpp`): оно случается один раз на загрузку, а всё здесь — каждый кадр.
namespace platformer {
namespace {

// Тайл в точке. Деление ЦЕЛОЧИСЛЕННОЕ по сырому Q16.16 и с округлением ВНИЗ — по тому же доводу,
// что в `TileGrid::window`: `fix32::operator/` усекает к нулю, и левее начала координат точка
// уезжала бы на тайл. Карта образца лежит в положительных координатах, но правило не про неё —
// про то, чтобы вопрос «какой тайл под ногами» не менял ответ от знака.
tm::TileFlags tile_at(const tm::TileGrid& g, Vec2 p) {
    const int64_t size = g.tile_size().raw;
    const int64_t dx = static_cast<int64_t>(p.x.raw) - g.origin().x.raw;
    const int64_t dy = static_cast<int64_t>(p.y.raw) - g.origin().y.raw;
    const int64_t tx = (dx >= 0 ? dx : dx - size + 1) / size;
    const int64_t ty = (dy >= 0 ? dy : dy - size + 1) / size;
    return g.at(static_cast<int32_t>(tx), static_cast<int32_t>(ty));
}

} // namespace

bool level_bounds(const Stage& st, LevelBounds& out) {
    if (!st.grid) return false;
    const Vec2 o = st.grid->origin();
    const fix32 ts = st.grid->tile_size();
    out.min = o;
    out.max = {o.x + ts * fix32::from_int(static_cast<int32_t>(st.grid->width())),
               o.y + ts * fix32::from_int(static_cast<int32_t>(st.grid->height()))};
    return true;
}

tm::TileFlags tile_at_point(const Stage& st, Vec2 p) {
    if (!st.grid) return tm::TILE_EMPTY;
    return tile_at(*st.grid, p);
}

namespace {

// Край уровня — свойство ОБРАЗЦА, а не движка: сетка кончается, и за её левой границей нет ни
// пола, ни стены, поэтому персонаж уходил в пустоту, оставаясь живым и управляемым. Находка
// владельческого прогона §6 от 2026-09-01 звучала ровно так: «пошёл налево за экран, герой пропал,
// а управление камерой осталось» — игра продолжалась ВНЕ уровня.
//
// Числа те же, которыми ограничена камера, и берутся они одним `level_bounds`. Полуширина корпуса
// вычитается, потому что ограничивается ТЕЛО, а не точка.
//
// Только по X, и это решение, а не недоделка: пол карты идёт во всю ширину, вниз выходить нечем, и
// поймай мы клампом падение сквозь него — мы спрятали бы дефект контроллера, а не закрыли дыру в
// уровне. Вверх карта открыта намеренно: прыжок выше верхнего ряда тайлов — это прыжок, а не выход
// из мира.
//
// Кламп ставит персонажа НА границу сетки, а граница — это край карты, а не обещание пустого места:
// сплошной столбец у самого края поставил бы корпус ВНУТРЬ тайла, и разбирал бы это перекрытие
// `move_and_slide` уже следующего кадра — то есть с опозданием и в сторону, которую выберет он, а не
// уровень. Поэтому после клампа персонаж отодвигается ВНУТРЬ уровня по тайлу за раз, пока корпус
// стоит в сплошном; шагов не больше ширины сетки — карта без единого свободного столбца уровнем не
// является, и вечного цикла на ней быть не должно.
void nudge_inside(const Stage& st, fix32 step_x, Vec2& p) {
    const ch::CollisionScene s = st.view();
    const ph::Shape hull = st.hull().shape;
    for (uint32_t i = 0; i < st.grid->width(); ++i) {
        tm::TileHit t;
        if (!tm::shapecast(*st.grid, hull, p, fix32{}, Vec2{}, s.tiles, t)) return;
        p.x = p.x + step_x;
    }
}

void clamp_to_level(Stage& st) {
    LevelBounds b;
    if (!level_bounds(st, b)) return;
    const fix32 left = b.min.x + HULL_HALF_W;
    const fix32 right = b.max.x - HULL_HALF_W;
    // Скорость гасится вместе с положением, и только та составляющая, которой персонаж в край и
    // упёрся: оставленная жить, она копилась бы всё время удержания кнопки и выстреливала бы
    // персонажем прочь от края в тот тик, когда её наконец разрешат применить.
    if (st.hero.position.x < left) {
        st.hero.position.x = left;
        if (st.hero.velocity.x.raw < 0) st.hero.velocity.x = fix32{};
        nudge_inside(st, st.grid->tile_size(), st.hero.position);
    } else if (right < st.hero.position.x) {
        st.hero.position.x = right;
        if (st.hero.velocity.x.raw > 0) st.hero.velocity.x = fix32{};
        nudge_inside(st, fix32{} - st.grid->tile_size(), st.hero.position);
    }
}

} // namespace

void step_stage(Stage& st, const ch::MoveInput& in) {
    // Разворот платформы — до шага мира, а не после: скорость, выставленная после интегрирования,
    // применилась бы только на следующем кадре, и платформа уезжала бы за свой маршрут на тик.
    // Ручка `mutate` действительна до ближайшего запроса или шага, поэтому берётся вплотную к нему.
    const fix32 x = st.world.body(st.lift).position.x;
    const fix32 v = st.world.body(st.lift).velocity.x;
    if ((x.raw >= LIFT_RIGHT.raw && v.raw > 0) || (x.raw <= LIFT_LEFT.raw && v.raw < 0))
        st.world.mutate(st.lift).velocity.x = fix32{} - v;

    st.world.step(tick_dt());
    ch::step(st.view(), st.hull(), st.profile, st.derived, in, tick_dt(), st.hero);
    clamp_to_level(st);
    // `crushed` — ЗАЯВЛЕНИЕ движка, а не решение: давить, выталкивать или терпеть, решает игра
    // (`push.hpp`). Образец возвращает персонажа в точку появления, потому что оставленный на месте
    // он остался бы стоять внутри давящей его платформы, и следующий кадр давил бы снова: находка
    // владельца сменила бы вид («выжимает наверх» → «застрял намертво»), а не закрылась.
    //
    // Флаг переживает возврат НАРОЧНО: появление обнуляет персонажа целиком, а прогон
    // (`platformer_sim.cpp`) читает `crushed` после кадра и обязан увидеть тот кадр, в котором
    // персонажа раздавило. Погашенный здесь, он сделал бы утверждение «маршрут никого не давит»
    // вечно верным — то есть вакуумным.
    if (st.hero.crushed) {
        place_at_spawn(st);
        st.hero.crushed = true;
    }
}

tm::TileFlags ground_tile(const Stage& st) {
    if (!st.hero.on_ground || !st.grid) return tm::TILE_EMPTY;
    // Точка пробы — на четверть тайла ниже подошвы: зазор `SKIN` мельче её, поэтому она попадает в
    // тайл под ногами и на плоском полу, и на склоне, где подошва стоит выше самой грани.
    const Vec2 under = {st.hero.position.x,
                        st.hero.position.y + HULL_HALF_H + fix32::from_int(4)};
    return tile_at(*st.grid, under);
}

} // namespace platformer
