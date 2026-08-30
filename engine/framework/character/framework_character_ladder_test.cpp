#include <cstdio>

#include "framework_character_ladder_scene.hpp"
#include "platform_args.hpp"

// Гейт ЛЕСТНИЦЫ (вертикаль 3, шаг D): лазание как отдельный режим движения и переходы машины
// состояний между ним, землёй и воздухом.
//
// Своей целью и своей раскладкой, потому что голден траектории идёт по сцене без единой лестницы и
// про неё не утверждает ничего, а вопрос здесь мерится ПОЗИЦИЕЙ и СОСТОЯНИЕМ, а не хешем: лестница,
// которая хватает без спроса или не отпускает по прыжку, даёт стабильный хеш на всех трёх машинах.
//
// Каждое утверждение парное. «Схватился» — правда и про персонажа, которого лестница притянула
// сама, поэтому рядом стоит прогон БЕЗ намерения, где он обязан пролететь шахту насквозь.
// «Отпустил прыжком» — правда и про того, кто вернулся на неё следующим тиком, поэтому рядом стоит
// прогон с окном перехвата в НОЛЬ, где возврат обязан случиться сразу.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void same(fix32 got, fix32 want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %.6f (raw %d), want %.6f (raw %d)\n", what, got.to_double(),
                got.raw, want.to_double(), want.raw);
    ++fails;
}

using namespace framework;
using namespace framework::character;


// 1. Хват требует НАМЕРЕНИЯ, и пара к нему — тот же прогон без него.
void test_grab() {
    Sim up(LADDER, 8, fx(40));
    settle(up);
    up.run(held(true, false, false), 1);
    check(up.c.state == MoveState::Ladder, "up on a ladder tile grabs it");

    Sim idle(LADDER, 8, fx(40));
    settle(idle);
    check(!idle.run_watching(held(false, false, false), 30), "standing still never grabs");

    // Падение сквозь шахту: лестница не тело, и без намерения она не ловит даже того, кто летит
    // через неё насквозь. Это же утверждение про `TILE_LADDER` без `TILE_SOLID`.
    Sim fall(LADDER, 8, fx(40));
    check(!fall.run_watching(held(false, false, false), 60), "falling through the shaft never grabs");
    check(fall.c.state == MoveState::Ground, "the faller reaches the floor");
    same(fall.c.position.y, floor_stand(), "the faller lands on the floor, not on the ladder");

    // Намерение БЕЗ лестницы: та же кнопка в соседней колонне не делает ничего.
    Sim aside(LADDER, 8, fx(40));
    aside.c.position.x = fx(40);
    settle(aside);
    check(!aside.run_watching(held(true, false, false), 30), "up away from a ladder grabs nothing");
}

// 2. На лестнице нет тяготения, а лазание идёт ПОСТОЯННОЙ скоростью профиля.
void test_climb() {
    Sim sim(LADDER, 8, fx(40));
    settle(sim);
    const fix32 base = sim.c.position.y;
    sim.run(held(true, false, false), 5);
    const fix32 climbed = base - sim.p.climb_speed * ladder_dt() * fx(5);
    same(sim.c.position.y, climbed, "five ticks up move exactly five steps of climb_speed");

    sim.run(held(false, false, false), 30);
    check(sim.c.state == MoveState::Ladder, "no input keeps him on the ladder");
    same(sim.c.position.y, climbed, "no input on a ladder means no gravity at all");

    sim.run(held(false, true, false), 5);
    same(sim.c.position.y, climbed + sim.p.climb_speed * ladder_dt() * fx(5),
         "down moves at the same speed as up");
}

// 3. Оба конца шахты: сверху сходит на площадку, снизу встаёт на пол.
void test_ends() {
    Sim top(LADDER, 8, fx(40));
    settle(top);
    top.run(held(true, false, false), 60);
    check(top.c.state != MoveState::Ladder, "the top of the shaft releases him");
    top.run(held(false, false, false), 30);
    check(top.c.on_ground, "released at the top, he lands on the landing");
    same(top.c.position.y, landing_stand(), "he stands on the landing, not inside it");

    // Спуск С ПЛОЩАДКИ: зонд под ногами, потому что центром стоящий на ней в пустоте.
    Sim down(LADDER, 8, fix32::from_int(6));
    settle(down);
    check(down.c.on_ground, "he starts standing on the landing");
    down.run(held(false, true, false), 1);
    check(down.c.state == MoveState::Ladder, "down on the landing grabs the ladder below it");
    down.run(held(false, true, false), 90);
    check(down.c.state == MoveState::Ground, "the bottom of the shaft puts him on the floor");
    same(down.c.position.y, floor_stand(), "he ends up standing on the floor");
}

// 4. Прыжок с лестницы и окно перехвата. Пара — то же самое с окном в ноль.
void test_jump_off() {
    Sim sim(LADDER, 8, fx(40));
    settle(sim);
    sim.run(held(true, false, false), 3);
    const fix32 hung = sim.c.position.y;
    sim.run(held(true, false, true), 1);
    check(sim.c.state == MoveState::Air, "jump releases the ladder");
    check(sim.c.position.y < hung, "he leaves the ladder upward");

    // Окно закрыто: та же зажатая «вверх» не возвращает его на ту же лестницу.
    for (uint32_t i = 0; i < sim.p.ladder_regrab_ticks; ++i) {
        sim.run(held(true, false, true), 1);
        check(sim.c.state == MoveState::Air, "the regrab window keeps him off the ladder");
    }
    sim.run(held(true, false, true), 1);
    check(sim.c.state == MoveState::Ladder, "past the window the same ladder is grabbed again");

    Sim none(LADDER, 0, fx(40));
    settle(none);
    none.run(held(true, false, false), 3);
    none.run(held(true, false, true), 1);
    check(none.c.state == MoveState::Air, "with no window the jump still releases");
    none.run(held(true, false, true), 1);
    check(none.c.state == MoveState::Ladder, "with no window he is back the very next tick");
}

// 5. Лестница ничего не держит: сквозь колонну ПРОХОДЯТ на бегу. Пара — та же раскладка, где шахта
// выложена сплошными тайлами: без неё «прошёл» было бы правдой и про персонажа, который просто
// добежал бы туда же по пустому уровню.
void test_not_solid() {
    MoveInput right = held(false, false, false);
    right.move_x = fix32::from_int(1);

    Sim through(LADDER, 8, fx(40));
    through.c.position.x = fx(40);
    settle(through);
    through.run(right, 60);
    check(fx(96) < through.c.position.x, "a ladder column is run through");

    Sim wall(tilemap::TILE_SOLID, 8, fx(40));
    wall.c.position.x = fx(40);
    settle(wall);
    wall.run(right, 60);
    check(wall.c.position.x < fx(80), "a solid column stops the same run");
    check(!wall.run_watching(held(true, false, false), 30), "a solid column is never climbed");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("character ladder mode\n");
    test_grab();
    test_climb();
    test_ends();
    test_jump_off();
    test_not_solid();
    std::printf("framework-character-ladder: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
