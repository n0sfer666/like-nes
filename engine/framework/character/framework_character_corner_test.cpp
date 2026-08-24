#include <cstdio>

#include "assist.hpp"
#include "controller.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"

// Гейт прощения УГЛА ПОТОЛКА (вертикаль 3): сдвиг случается ровно в заявленном окне — на границе,
// за ней и при нулевом, — и ровно при заявленных условиях.
//
// Отдельной целью от гейта окон, хотя вопрос того же рода: там окно меряется ТИКАМИ и наблюдается
// на общей сцене, здесь — ЮНИТАМИ и требует своей геометрии (потолок с проёмом, столб на пути
// сдвига, стена). Дописать её в общую сцену значило бы менять обстановку под голденом ради вопроса,
// который голден не задаёт, — то же основание, по которому своя сцена у гейта туннелирования.
//
// Гейт нужен ещё и потому, что голден траектории после этого приёма НЕ СДВИНУЛСЯ: в его сценарии
// приём не срабатывает ни разу. То есть без этого файла его не проверяет НИЧТО, и мёртвый код
// выглядел бы как рабочий.
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

// Потолок с проёмом: плита y [-64, -48], в ней дыра x [0, 32].
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
fix32 corner_shift(const CollisionScene& s, const CharacterHull& hull, fix32 window, Vec2 travel,
                   Vec2 from) {
    Vec2 p = from;
    corner_correct(s, hull, travel, window, p);
    return p.x - from.x;
}

void test_corner_window() {
    const tilemap::TileGrid g = make_ceiling();
    const CollisionScene s{nullptr, &g};
    const CharacterHull h = make_hull();
    const fix32 at_window = corner_shift(s, h, NEEDED_SHIFT, RISE, UNDER_CORNER);
    const fix32 below = corner_shift(s, h, NEEDED_SHIFT - fix32::from_int(1), RISE, UNDER_CORNER);
    const fix32 wide = corner_shift(s, h, MAX_CORNER_CORRECTION, RISE, UNDER_CORNER);
    const fix32 zero = corner_shift(s, h, fix32{}, RISE, UNDER_CORNER);
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
    // склонами шага B. Та же плита и тот же промах, различается ровно знак пути.
    const tilemap::TileGrid g = make_ceiling();
    const CollisionScene s{nullptr, &g};
    const CharacterHull h = make_hull();
    const Vec2 above = {UNDER_CORNER.x, fix32::from_int(-100)};
    const Vec2 fall = {fix32{}, fix32::from_int(40)};
    check(corner_shift(s, h, MAX_CORNER_CORRECTION, fall, above).raw == 0,
          "the same overhang met while falling is not forgiven");
    // Ход строго по горизонтали головой вплотную к плите: бегущий под низким потолком не обязан
    // уезжать вбок. Оба случая держит условие НОРМАЛИ, а не ранний выход по знаку пути, — снятый
    // знак этот гейт не ловит, и в `assist.hpp` записано именно так.
    const Vec2 touching = {UNDER_CORNER.x, fix32::from_int(-32)};
    const Vec2 walk = {fix32::from_int(5), fix32{}};
    check(corner_shift(s, h, MAX_CORNER_CORRECTION, walk, touching).raw == 0,
          "running flat with the head against the slab shifts nobody");
}

// Узкий корпус: только у него сдвиг МОЖЕТ перепрыгнуть препятствие. У корпуса шире окна начальное и
// конечное положения перекрываются, заметённая область равна их объединению, и «путь занят» тогда
// неотличимо от «конец занят» — то есть проверку пути не наблюдал бы ни один случай.
constexpr fix32 NARROW_HALF_W = fix32::from_int(2);

CharacterHull narrow_hull() {
    CharacterHull h;
    h.shape = physics::sanitize(physics::box(NARROW_HALF_W, HULL_HALF_H));
    return h;
}

// Столб ТЕЛОМ: он обязан быть уже тайла, а тайл — единица сетки.
struct Mixed {
    physics::World world;
    tilemap::TileGrid grid;

    CollisionScene view() const { return {&world, &grid}; }
};

Mixed make_column(bool with_column) {
    physics::World w(2);
    w.set_gravity({fix32{}, fix32{}});
    if (with_column)
        add_static(w, 1, {fix32::from_float(-2.5), fix32{}}, fix32::from_float(0.5),
                   fix32::from_int(20));
    return {std::move(w), make_ceiling()};
}

void test_corner_refuses_a_crossed_column() {
    // Узкий корпус x [-8.5, -4.5] заходит под плиту на 8.5, значит освобождает подъём сдвиг на 9 —
    // больше собственной ширины. Столб x [-3, -2] лежит ЦЕЛИКОМ между началом и концом сдвига: конец
    // свободен, а путь — нет. Ровно это и разделяет свип пути от проверки конечной точки.
    const Vec2 from = {fix32::from_float(-6.5), fix32{}};
    const CharacterHull h = narrow_hull();
    const Mixed clear = make_column(false);
    const Mixed blocked = make_column(true);
    const fix32 free_shift = corner_shift(clear.view(), h, MAX_CORNER_CORRECTION, RISE, from);
    const fix32 crossed = corner_shift(blocked.view(), h, MAX_CORNER_CORRECTION, RISE, from);
    std::printf("  column: free=%.1f crossed=%.1f\n", free_shift.to_double(), crossed.to_double());
    check(free_shift == fix32::from_int(9), "control: without the column the narrow hull clears");
    check(crossed.raw == 0, "a shift whose path crosses a column is refused, not taken");
}

// Клин с наклонной НИЖНЕЙ гранью: от (-20, -100) до (20, -20), то есть 63 градуса к горизонту.
// Осевой стеной этот случай не строится вовсе: вертикальный свип к вертикальной грани не
// приближается и ответить ею может лишь из касания, которого при зазоре SKIN не бывает. Наклонная
// грань отвечает на любом зазоре — и ровно ради неё, то есть ради склонов шага B, проверка нормали
// и заведена.
void add_wedge(physics::World& w, Vec2 center) {
    const Vec2 tri[3] = {{fix32::from_int(-20), fix32::from_int(-40)},
                         {fix32::from_int(20), fix32::from_int(-40)},
                         {fix32::from_int(20), fix32::from_int(40)}};
    physics::BodyDesc d;
    d.key = 7;
    d.type = physics::BodyType::Static;
    d.shape = physics::polygon(tri, 3);
    d.position = center;
    w.add(d);
}

void test_corner_refuses_a_steep_ceiling() {
    physics::World w(2);
    w.set_gravity({fix32{}, fix32{}});
    add_wedge(w, {fix32{}, fix32::from_int(-60)});
    const CollisionScene s{&w, nullptr};
    const CharacterHull h = make_hull();
    const Vec2 from = {fix32{}, fix32::from_int(-20)};
    const Vec2 rise = {fix32{}, fix32::from_int(-30)};
    SceneHit hit;
    check(cast_nearest(s, h, from, rise, hit) && hit.normal.y.raw > 0 &&
              !(SURFACE_NORMAL_Y < hit.normal.y),
          "control: the rise answers with a face too steep to be a ceiling");
    // Сдвиг ВЛЕВО просвет увеличивает — грань уходит вверх, — то есть приём без проверки нормали
    // нашёл бы куда уехать и растащил бы персонажа вдоль склона, вдоль которого он и так скользит.
    check(corner_shift(s, h, MAX_CORNER_CORRECTION, rise, from).raw == 0,
          "a steep face met on the way up is not a ceiling corner");
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
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character corner-correction gate\n");
    test_corner_window();
    test_corner_is_the_head_only();
    test_corner_refuses_a_crossed_column();
    test_corner_refuses_a_steep_ceiling();
    test_corner_is_wired_into_the_tick();
    std::printf("framework-character-corner: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
