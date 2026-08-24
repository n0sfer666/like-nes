#include <cstdio>

#include "assist.hpp"
#include "controller.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"

// Гейт геометрического прощения (вертикаль 3): сдвиг из-под угла потолка и притяжение к полу
// срабатывают РОВНО в заявленном окне — на границе, за ней и при нулевом окне.
//
// Отдельной целью от гейта окон, хотя вопрос того же рода: там окно меряется ТИКАМИ и наблюдается
// на общей сцене, здесь — ЮНИТАМИ и требует своей геометрии (потолок с проёмом, ступенька вниз).
// Дописать её в общую сцену значило бы менять обстановку под голденом ради вопроса, который голден
// не задаёт, — то же основание, по которому своя сцена у гейта туннелирования.
//
// Гейт нужен ещё и потому, что голден траектории после этих двух приёмов НЕ СДВИНУЛСЯ: в его
// сценарии ни один из них не срабатывает ни разу. То есть без этого файла оба приёма не проверяет
// НИЧТО, и мёртвый код выглядел бы как рабочий.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::character;

fix32 tick_dt() { return fix32::from_int(1) / fix32::from_int(60); }

// Потолок с проёмом: плита y [-64, -48], в ней дыра x [0, 32]. Хитбокс шириной 16, значит в проёме
// у него по 8 юнитов свободы с каждой стороны — сдвиг обязан попадать ВНУТРЬ проёма, а не в его
// грань, иначе «прошёл» решал бы допуск свипа.
tilemap::TileGrid make_ceiling() {
    tilemap::TileGrid g({fix32::from_int(-160), fix32::from_int(-160)}, fix32::from_int(16), 20, 20);
    g.fill(0, 6, 10, 7, tilemap::TILE_SOLID);
    g.fill(12, 6, 20, 7, tilemap::TILE_SOLID);
    return g;
}

// Промах ДРОБНЫЙ, и это несущее: при целом промахе сдвиг ровно на него ставит грань хитбокса грань
// в грань с проёмом, и ответ приходил бы от допуска свипа, а не от окна. Левый край заходит под
// плиту на 2.5 юнита, поэтому сдвиг на 2 оставляет 0.5 перекрытия, а сдвиг на 3 даёт 0.5 просвета.
constexpr fix32 OVERHANG = fix32::from_float(2.5);
constexpr fix32 NEEDED_SHIFT = fix32::from_int(3);
const Vec2 UNDER_CORNER = {HULL_HALF_W - OVERHANG, fix32{}};
const Vec2 RISE = {fix32{}, fix32::from_int(-40)};

// Сдвиг, который приём сделал при данном окне. Несработавший оставляет позицию нетронутой, то есть
// отвечает нулём.
fix32 corner_shift(const CollisionScene& s, fix32 window, Vec2 travel, Vec2 from) {
    Vec2 p = from;
    corner_correct(s, make_hull(), travel, window, p);
    return p.x - from.x;
}

void test_corner_window() {
    const tilemap::TileGrid g = make_ceiling();
    const CollisionScene s{nullptr, &g};
    const fix32 at_window = corner_shift(s, NEEDED_SHIFT, RISE, UNDER_CORNER);
    const fix32 below = corner_shift(s, NEEDED_SHIFT - fix32::from_int(1), RISE, UNDER_CORNER);
    const fix32 wide = corner_shift(s, MAX_CORNER_CORRECTION, RISE, UNDER_CORNER);
    const fix32 zero = corner_shift(s, fix32{}, RISE, UNDER_CORNER);
    std::printf("  corner: needed=%.1f at=%.1f below=%.1f wide=%.1f zero=%.1f\n",
                NEEDED_SHIFT.to_double(), at_window.to_double(), below.to_double(),
                wide.to_double(), zero.to_double());
    check(at_window == NEEDED_SHIFT, "the corner clears on the last unit of its window");
    check(below.raw == 0, "one unit short of it clears nothing");
    check(zero.raw == 0, "a zero window shifts nobody");
    // Окно — ПОТОЛОК сдвига, а не его величина: приём обязан брать наименьший подходящий. Иначе
    // персонаж с широким окном перелетал бы проём насквозь, и проход стоил бы телепорта.
    check(wide == NEEDED_SHIFT, "a wide window still takes the smallest shift that works");
}

void test_corner_is_the_head_only() {
    // Решение владельца 2026-08-24: прощается ТОЛЬКО голова, ступенька под ногами приезжает со
    // склонами шага B. Та же плита и тот же промах, различается ровно знак пути. Утверждение здесь
    // про НАБЛЮДАЕМОЕ: ноги не прощаются — держат его два условия сразу, направление и нормаль.
    const tilemap::TileGrid g = make_ceiling();
    const CollisionScene s{nullptr, &g};
    const Vec2 above = {UNDER_CORNER.x, fix32::from_int(-100)};
    const Vec2 fall = {fix32{}, fix32::from_int(40)};
    check(corner_shift(s, MAX_CORNER_CORRECTION, fall, above).raw == 0,
          "the same overhang met while falling is not forgiven");
    // Ход строго по горизонтали головой вплотную к плите: бегущий под низким потолком не обязан
    // уезжать вбок. Оба случая держит условие НОРМАЛИ, а не ранний выход по знаку пути, — снятый
    // знак этот гейт не ловит, и в `assist.hpp` записано именно так.
    const Vec2 touching = {UNDER_CORNER.x, fix32::from_int(-32)};
    const Vec2 walk = {fix32::from_int(5), fix32{}};
    check(corner_shift(s, MAX_CORNER_CORRECTION, walk, touching).raw == 0,
          "running flat with the head against the slab shifts nobody");
}

// Подъём под угол ТИКОМ контроллера. Возвращает, ушёл ли персонаж выше плиты.
bool rises_through(fix32 window, bool& hit_ceiling) {
    MoveProfile p = default_profile();
    p.corner_correction = window;
    p = sanitize(p);
    const MoveDerived d = derive(p, tick_dt());
    const tilemap::TileGrid g = make_ceiling();
    const CollisionScene s{nullptr, &g};
    Character c;
    c.position = UNDER_CORNER;
    c.velocity = {fix32{}, fix32::from_int(-600)};
    hit_ceiling = false;
    for (uint32_t t = 0; t < 20; ++t) {
        step(s, make_hull(), p, d, MoveInput{}, tick_dt(), c);
        hit_ceiling = hit_ceiling || c.hit_ceiling;
    }
    return c.position.y < fix32::from_int(-80);
}

void test_corner_is_wired_into_the_tick() {
    // Таблица выше зовёт приём напрямую и потому не говорит, зовёт ли его ТИК и при тех ли
    // условиях. Контроль — тот же подъём с выключенным окном: он обязан упереться головой.
    bool bumped_wide = false, bumped_off = false;
    const bool through = rises_through(fix32::from_int(4), bumped_wide);
    const bool stopped = rises_through(fix32{}, bumped_off);
    check(through && !bumped_wide, "the tick applies the correction and the head clears the slab");
    check(!stopped && bumped_off, "control: the same rise without the window hits the ceiling");
}

// Ступенька: верхняя площадка ТАЙЛАМИ (верх y = 0, x [-160, 0]), нижняя — ТЕЛОМ, потому что её верх
// обязан стоять на дробной высоте. Дробной по той же причине, что и промах у потолка: при целой
// глубине граница окна упиралась бы в допуск свипа.
struct Step {
    physics::World world;
    tilemap::TileGrid grid;
    CollisionScene view() const { return {&world, &grid}; }
};

Step make_step(fix32 drop) {
    physics::World w(2);
    w.set_gravity({fix32{}, fix32{}});
    add_static(w, 1, {fix32::from_int(80), drop + fix32::from_int(80)}, fix32::from_int(80),
               fix32::from_int(80));
    tilemap::TileGrid g({fix32::from_int(-160), fix32{}}, fix32::from_int(16), 10, 10);
    g.fill(0, 0, 10, 10, tilemap::TILE_SOLID);
    return {std::move(w), std::move(g)};
}

constexpr fix32 STEP_DROP = fix32::from_float(5.5);
constexpr fix32 NEEDED_SNAP = fix32::from_int(6);   // 5.5 глубины плюс зазор SKIN, вверх до целого

fix32 snap_drop(const CollisionScene& s, fix32 window) {
    Vec2 p = {fix32::from_int(20), FLOOR_TOP - HULL_HALF_H - SKIN};
    const fix32 from = p.y;
    snap_to_ground(s, make_hull(), window, p);
    return p.y - from;
}

void test_snap_window() {
    const Step st = make_step(STEP_DROP);
    const fix32 at_window = snap_drop(st.view(), NEEDED_SNAP);
    const fix32 below = snap_drop(st.view(), NEEDED_SNAP - fix32::from_int(1));
    const fix32 zero = snap_drop(st.view(), fix32{});
    const Step deep = make_step(fix32::from_int(20) + STEP_DROP);
    const fix32 too_deep = snap_drop(deep.view(), default_profile().ground_snap);
    std::printf("  snap: drop=%.3f at=%.3f below=%.3f zero=%.3f too_deep=%.3f\n",
                STEP_DROP.to_double(), at_window.to_double(), below.to_double(), zero.to_double(),
                too_deep.to_double());
    // Притянутый встаёт на тот же зазор, что и после разбора касания, — не «куда-нибудь вниз».
    // Порогом «сдвинулся» это не проверяется: сдвиг на пол-юнита прошёл бы его тоже.
    check(at_window == STEP_DROP, "the snap lands the character on the lower floor exactly");
    check(below.raw == 0, "one unit short of the drop pulls nobody");
    check(zero.raw == 0, "a zero window pulls nobody");
    check(too_deep.raw == 0, "a drop past the window is a fall, not a step");
}

// Пройти вправо через край ступеньки. Возвращает, терялась ли опора хоть на тик.
bool loses_ground(fix32 window) {
    MoveProfile p = default_profile();
    p.ground_snap = window;
    p = sanitize(p);
    const MoveDerived d = derive(p, tick_dt());
    const Step st = make_step(STEP_DROP);
    Character c = standing_at(fix32::from_int(-40));
    bool lost = false;
    for (uint32_t t = 0; t < 20; ++t) {
        MoveInput in;
        in.move_x = fix32::from_int(1);
        step(st.view(), make_hull(), p, d, in, tick_dt(), c);
        lost = lost || !c.on_ground;
    }
    check(HULL_HALF_W < c.position.x, "the walk actually crosses the step edge");
    return lost;
}

void test_snap_is_wired_into_the_tick() {
    check(!loses_ground(default_profile().ground_snap), "walking off the step keeps the ground");
    check(loses_ground(fix32{}), "control: the same step without the window drops the character");
}

void test_the_jump_is_untouched() {
    // Прыжок исключён ПО ПОСТРОЕНИЮ — первый тик даёт `velocity.y < 0`, — и это утверждение, а не
    // комментарий: приём, спрошенный без проверки знака, притянул бы персонажа обратно к полу тем
    // же тиком, и прыжок исчез бы целиком. Окно взято МАКСИМАЛЬНОЕ: чем шире, тем громче поломка.
    MoveProfile p = default_profile();
    p.ground_snap = MAX_GROUND_SNAP;
    p = sanitize(p);
    const MoveDerived d = derive(p, tick_dt());
    const Step st = make_step(STEP_DROP);
    Character c = standing_at(fix32::from_int(-40));
    fix32 top = c.position.y;
    for (uint32_t t = 0; t < 40; ++t) {
        MoveInput in;
        in.jump_held = t < 30;
        step(st.view(), make_hull(), p, d, in, tick_dt(), c);
        top = min_fix(top, c.position.y);
    }
    const fix32 risen = (FLOOR_TOP - HULL_HALF_H - SKIN) - top;
    std::printf("  jump under the widest snap: apex=%.3f target=%.3f\n", risen.to_double(),
                p.jump_height.to_double());
    check(p.jump_height - fix32::from_int(1) < risen, "the widest ground snap leaves the jump alone");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character assist gate\n");
    test_corner_window();
    test_corner_is_the_head_only();
    test_corner_is_wired_into_the_tick();
    test_snap_window();
    test_snap_is_wired_into_the_tick();
    test_the_jump_is_untouched();
    std::printf("framework-character-assist: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
