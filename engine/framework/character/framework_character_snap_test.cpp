#include <cstdio>

#include "assist.hpp"
#include "controller.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"

// Гейт ПРИТЯЖЕНИЯ К ЗЕМЛЕ на спусках (вертикаль 3): притяжение случается ровно в заявленном окне —
// на границе, за ней и при нулевом, — и не трогает прыжок.
//
// Своей целью и своей геометрией по тем же двум причинам, что и гейт угла потолка: окно меряется
// ЮНИТАМИ, а голден траектории на этом приёме не срабатывает ни разу и потому не проверяет его.
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

struct Mixed {
    physics::World world;
    tilemap::TileGrid grid;

    CollisionScene view() const { return {&world, &grid}; }
};

// Ступенька: верхняя площадка ТАЙЛАМИ (верх y = 0, x [-160, 0]), нижняя — ТЕЛОМ, потому что её верх
// обязан стоять на дробной высоте. Дробной по той же причине, что и промах у потолка в соседнем
// гейте: при целой глубине граница окна упиралась бы в допуск свипа.
Mixed make_step(fix32 drop) {
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
const Vec2 STANDING = {fix32::from_int(20), FLOOR_TOP - HULL_HALF_H - SKIN};

fix32 snap_drop(const CollisionScene& s, fix32 window) {
    Vec2 p = STANDING;
    snap_to_ground(s, make_hull(), window, p);
    return p.y - STANDING.y;
}

void test_snap_window() {
    const Mixed st = make_step(STEP_DROP);
    const fix32 at_window = snap_drop(st.view(), NEEDED_SNAP);
    const fix32 below = snap_drop(st.view(), NEEDED_SNAP - fix32::from_int(1));
    const fix32 zero = snap_drop(st.view(), fix32{});
    const Mixed deep = make_step(fix32::from_int(20) + STEP_DROP);
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

// Клин с наклонной ВЕРХНЕЙ гранью: от (-20, 100) до (20, 20), то есть 63 градуса к горизонту —
// круче порога `SURFACE_NORMAL_Y`, значит стоять на нём нельзя. Осевой стеной этот случай не
// строится: вертикальный свип к вертикальной грани не приближается. Наклонная отвечает на любом
// зазоре — и ровно ради неё, то есть ради склонов шага B, проверка нормали и заведена.
void add_wedge(physics::World& w, Vec2 center) {
    const Vec2 tri[3] = {{fix32::from_int(-20), fix32::from_int(40)},
                         {fix32::from_int(20), fix32::from_int(40)},
                         {fix32::from_int(20), fix32::from_int(-40)}};
    physics::BodyDesc d;
    d.key = 7;
    d.type = physics::BodyType::Static;
    d.shape = physics::polygon(tri, 3);
    d.position = center;
    w.add(d);
}

void test_snap_refuses_a_steep_slope() {
    physics::World w(2);
    w.set_gravity({fix32{}, fix32{}});
    add_wedge(w, {fix32{}, fix32::from_int(60)});
    const CollisionScene s{&w, nullptr};
    const CharacterHull h = make_hull();
    Vec2 p{fix32{}, fix32::from_int(20)};
    SceneHit hit;
    check(cast_nearest(s, h, p, {fix32{}, MAX_GROUND_SNAP}, hit) && hit.normal.y.raw < 0 &&
              !(hit.normal.y < -SURFACE_NORMAL_Y),
          "control: the downward sweep answers with a face too steep to stand on");
    check(!snap_to_ground(s, h, MAX_GROUND_SNAP, p), "a steep face underfoot is not ground");
    // Приём двигает позицию по НОРМАЛИ, и у склона она наклонена: без проверки персонаж не просто
    // «встал» бы на непроходимое, а уехал бы вбок — то есть выдал бы спуск по стене за спуск.
    check(p.x.raw == 0 && p.y == fix32::from_int(20), "and a refused snap leaves the position alone");
}

// Пройти вправо через край ступеньки. Возвращает, терялась ли опора хоть на тик.
bool loses_ground(fix32 window) {
    MoveProfile p = default_profile();
    p.ground_snap = window;
    p = sanitize(p);
    const MoveDerived d = derive(p, tick_dt());
    const Mixed st = make_step(STEP_DROP);
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

// Прыжок с широчайшим окном притяжения. `clearance` — просвет над головой; ноль означает открытое
// небо. Возвращает высоту, на которую персонаж поднялся над положением стоя.
fix32 jump_apex(fix32 clearance) {
    MoveProfile p = default_profile();
    p.ground_snap = MAX_GROUND_SNAP;
    p.corner_correction = fix32{};   // сдвиг вбок увёл бы персонажа из-под потолка и спас бы прыжок
    p = sanitize(p);
    const MoveDerived d = derive(p, tick_dt());
    Mixed st = make_step(STEP_DROP);
    if (clearance.raw != 0) {
        const fix32 head = FLOOR_TOP - HULL_HALF_H - SKIN - HULL_HALF_H;
        add_static(st.world, 2, {fix32{}, head - clearance - fix32::from_int(20)},
                   fix32::from_int(200), fix32::from_int(20));
    }
    Character c = standing_at(fix32::from_int(-40));
    fix32 top = c.position.y;
    for (uint32_t t = 0; t < 40; ++t) {
        MoveInput in;
        in.jump_held = t < 30;
        step(st.view(), make_hull(), p, d, in, tick_dt(), c);
        top = min_fix(top, c.position.y);
    }
    return (FLOOR_TOP - HULL_HALF_H - SKIN) - top;
}

void test_the_jump_is_untouched() {
    // Прыжок исключён снимком «поднимался», снятым ДО движения, — и это утверждение, а не
    // комментарий: приём, спрошенный без него, притянул бы персонажа обратно к полу тем же тиком.
    // Окно взято МАКСИМАЛЬНОЕ: чем шире, тем громче поломка.
    const fix32 open = jump_apex(fix32{});
    const MoveProfile p = sanitize(default_profile());
    std::printf("  jump under the widest snap: apex=%.3f target=%.3f\n", open.to_double(),
                p.jump_height.to_double());
    check(p.jump_height - fix32::from_int(1) < open, "the widest ground snap leaves the jump alone");
}

void test_the_jump_survives_a_low_ceiling() {
    // Прыжок, у которого просвет над головой МЕНЬШЕ одного тика подъёма: скорость гасится о потолок
    // тем же тиком, и условие «не поднимается», прочитанное по скорости ПОСЛЕ движения, пропускало
    // притяжение — персонаж возвращался на пол, и прыжка не случалось вовсе. Окно дефекта считалось
    // числами профиля (392 юнита/с ÷ 60 ≈ 6.5), а не редкой геометрией.
    const fix32 tick_rise = fix32::from_int(392) / fix32::from_int(60);
    const fix32 clearance = fix32::from_int(4);
    const fix32 low = jump_apex(clearance);
    std::printf("  jump under a %.1f-unit ceiling: apex=%.3f (a tick of rise is %.3f)\n",
                clearance.to_double(), low.to_double(), tick_rise.to_double());
    check(low < tick_rise, "precondition: the ceiling really is closer than one tick of rise");
    check(fix32::from_int(3) < low, "a jump that clips a low ceiling still leaves the ground");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character ground-snap gate\n");
    test_snap_window();
    test_snap_refuses_a_steep_slope();
    test_snap_is_wired_into_the_tick();
    test_the_jump_is_untouched();
    test_the_jump_survives_a_low_ceiling();
    std::printf("framework-character-snap: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
