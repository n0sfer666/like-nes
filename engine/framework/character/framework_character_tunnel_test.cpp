#include <cstdio>
#include <vector>

#include "controller.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"

// Инвариант 2 спеки #16: на максимальной скорости персонажа сквозь тонкую стену прохода нет.
//
// Сцена своя, а не общая: вопрос требует ЗАВЕДОМО тонкой стены — тоньше, чем путь за тик, — и в
// общей сцене такая стена меняла бы траекторию голдена ради вопроса, который голден не задаёт.
// Хитбокс при этом берётся ОБЩИЙ (`make_hull`): персонаж у всех гейтов один, и «не прошёл сквозь
// стену» на другом размере коробки не говорило бы о том персонаже, чей голден пинится рядом.
//
// Гейт несёт ПОЗИТИВНЫЙ КОНТРОЛЬ и без него был бы вакуумным: «персонаж не прошёл сквозь стену»
// одинаково верно и для стены, которую невозможно пробить, и для стены, до которой он не долетел.
// Контроль — телепорт, то есть движение БЕЗ шейпкаста: он обязан пройти насквозь той же сценой и
// тем же вводом, иначе гейт проверяет расстояние, а не свип.
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

constexpr fix32 WALL_X = fix32::from_int(200);
// Полтолщины вдвое меньше кванта позиции быть не может, но и «толстой» стена тут быть не должна:
// при 2048 юнит/с шаг за тик равен 34 юнитам, то есть стена в полюнита толщиной прошивается любым
// движением «сдвинуть и проверить».
constexpr fix32 WALL_HALF = fix32::from_float(0.25);

physics::World make_thin_wall() {
    physics::World w(4);
    w.set_gravity({fix32{}, fix32{}});
    physics::BodyDesc d;
    d.key = 1;
    d.type = physics::BodyType::Static;
    d.shape = physics::box(WALL_HALF, fix32::from_int(200));
    d.position = {WALL_X, fix32{}};
    w.add(d);
    return w;
}

MoveProfile flying_profile() {
    // Тяготение снято, потолок скорости поднят до потолка физики: вопрос гейта — свип на предельной
    // скорости, а падение по дороге к стене только добавило бы к нему второй.
    MoveProfile p = default_profile();
    p.gravity_rise = fix32{};
    p.gravity_fall = fix32{};
    p.max_speed = MAX_MOVE_SPEED;
    p.ground_accel = MAX_MOVE_ACCEL;
    p.air_accel = MAX_MOVE_ACCEL;
    return sanitize(p);
}

void test_sweep_stops_at_the_wall() {
    const MoveProfile p = flying_profile();
    const MoveDerived d = derive(p, tick_dt());
    physics::World w = make_thin_wall();
    const CharacterHull hull = make_hull();
    Character c;
    c.position = {fix32::from_int(-40), fix32{}};
    fix32 top_speed{};
    for (uint32_t t = 0; t < 60; ++t) {
        MoveInput in;
        in.move_x = fix32::from_int(1);
        step(w, hull, p, d, in, tick_dt(), c);
        top_speed = max_fix(top_speed, c.velocity.x);
        // Сверять с ЦЕНТРОМ стены значило бы разрешить восемь юнитов проникновения: у хитбокса
        // полуширина 8, и «центр не пересечён» верно ещё и когда персонаж наполовину в стене.
        // Утверждение о непрохождении обязано стоять на ГРАНИ, до которой доезжает край хитбокса.
        check(c.position.x < WALL_X - WALL_HALF - HULL_HALF_W,
              "the character never crosses the wall face");
        if (fails > 0) break;
    }
    // Разгон обязан состояться: гейт на персонаже, который так и не набрал скорость, проверяет
    // отсутствие движения, а не отсутствие туннелирования.
    const fix32 per_tick = top_speed * tick_dt();
    std::printf("  top speed=%.1f, %.1f units per tick, wall half=%.2f\n", top_speed.to_double(),
                per_tick.to_double(), WALL_HALF.to_double());
    check(WALL_HALF * fix32::from_int(2) < per_tick, "one tick of travel outruns the wall thickness");
}

void test_teleport_control_passes_through() {
    const MoveProfile p = flying_profile();
    physics::World w = make_thin_wall();
    const CharacterHull hull = make_hull();
    Vec2 position = {fix32::from_int(-40), fix32{}};
    const Vec2 travel = {p.max_speed * tick_dt(), fix32{}};
    bool crossed = false;
    for (uint32_t t = 0; t < 60 && !crossed; ++t) {
        // Тот самый «сдвинуть и проверить»: позиция меняется без свипа, столкновение ищется уже
        // ПОСЛЕ сдвига. Именно так стена и теряется.
        position = position + travel;
        physics::QueryFilter f;
        f.mask = hull.mask;
        std::vector<physics::Overlap> hits;
        physics::overlap_shape(w, hull.shape, position, fix32{}, f, hits);
        crossed = hits.empty() && WALL_X < position.x;
    }
    check(crossed, "the teleport control does tunnel through the same wall");
}

// Внутренний угол: пол снизу, стена слева. Вопрос — не «остановился ли», а «остался ли подвижен»:
// безусловный отход на зазор (`slide.cpp`) существует ровно затем, чтобы персонаж, разобравший ДВА
// касания за один тик, не оказался вжат ни в одну из поверхностей. Вжатый — это не косметика: свип,
// стартующий в допуске, отдаёт долю пути ноль в ЛЮБУЮ сторону, включая сторону ОТ поверхности, и
// именно так был потерян прыжок.
//
// Позитивный контроль внутри самого случая: тот же персонаж, поставленный ровно в допуск свипа
// вместо зазора, обязан НЕ сдвинуться тем же движением. Без него «сдвинулся на 4 юнита» проверяло
// бы, что свип вообще работает, а не что зазор что-то держит.
physics::World make_inner_corner() {
    physics::World w(4);
    w.set_gravity({fix32{}, fix32{}});
    physics::BodyDesc d;
    d.key = 1;
    d.type = physics::BodyType::Static;
    d.shape = physics::box(fix32::from_int(300), fix32::from_int(100));
    d.position = {fix32::from_int(300), fix32::from_int(100)};
    w.add(d);
    d.key = 2;
    d.shape = physics::box(fix32::from_int(10), fix32::from_int(100));
    d.position = {fix32::from_int(-10), fix32::from_int(-100)};
    w.add(d);
    return w;
}

fix32 travelled(const physics::World& w, const CharacterHull& hull, Vec2 from, Vec2 travel) {
    Vec2 position = from;
    Vec2 velocity{};
    move_and_slide(w, hull, travel, position, velocity);
    Vec2 dir{};
    return normalize(position - from, dir);
}

void test_corner_clearance() {
    physics::World w = make_inner_corner();
    const CharacterHull hull = make_hull();
    Vec2 position = {fix32::from_int(60), fix32::from_int(-17)};
    Vec2 velocity = {fix32::from_int(-100), fix32::from_int(50)};
    move_and_slide(w, hull, {fix32::from_int(-100), fix32::from_int(50)}, position, velocity);

    // Угол разобран: персонаж стоит на полу у стены, а не внутри них.
    check(HULL_HALF_W < position.x && position.x < HULL_HALF_W + fix32::from_int(1),
          "the corner leaves the character clear of the wall");
    check(position.y < -HULL_HALF_H && -HULL_HALF_H - fix32::from_int(1) < position.y,
          "and clear of the floor");

    const Vec2 up = {fix32{}, fix32::from_int(-4)};
    const Vec2 away = {fix32::from_int(4), fix32{}};
    const fix32 rose = travelled(w, hull, position, up);
    const fix32 slid = travelled(w, hull, position, away);
    std::printf("  corner: up=%.3f, away=%.3f\n", rose.to_double(), slid.to_double());
    check(fix32::from_float(3.9) < rose, "and free to leave the floor");
    check(fix32::from_float(3.9) < slid, "and free to leave the wall");

    // Контроль: тот же персонаж в ДОПУСКЕ свипа вместо зазора никуда не уезжает.
    const Vec2 pinched = {HULL_HALF_W + physics::CONTACT_SLOP,
                          -HULL_HALF_H - physics::CONTACT_SLOP};
    const fix32 pinched_up = travelled(w, hull, pinched, up);
    std::printf("  pinched control: up=%.3f\n", pinched_up.to_double());
    check(pinched_up < fix32::from_int(1), "the pinched control cannot move at all");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character tunnel gate\n");
    test_sweep_stops_at_the_wall();
    test_teleport_control_passes_through();
    test_corner_clearance();
    std::printf("framework-character-tunnel: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
