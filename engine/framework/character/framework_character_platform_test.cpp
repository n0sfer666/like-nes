#include <cstdio>

#include "framework_character_platform_scene.hpp"
#include "platform_args.hpp"

// Гейт ПЕРЕНОСА ОПОРОЙ (вертикаль 3, шаг B3): персонаж, стоящий на движущейся платформе, едет
// вместе с ней, прыгает с неё, вжимается в потолок и забывает её, сойдя с края.
//
// Своей целью и своей раскладкой (`framework_character_platform_scene.hpp`) по той же причине, что
// у склона и односторонней платформы: голден траектории идёт по сцене, где мир не шагается НИ РАЗУ,
// то есть про движущуюся геометрию не утверждает ничего, а вопрос здесь мерится не хешем, а
// сравнением пути персонажа с путём его опоры.
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

fix32 fx(int v) { return fix32::from_int(v); }

// Сравнение путей — с допуском в 1/64 юнита: перенос складывает те же `velocity * dt`, что и
// интеграция мира, поэтому расхождение обязано быть нулевым, а полоса стоит здесь затем, чтобы
// падение читалось как «поехал не с той скоростью», а не как «разошлось на квант».
bool same(fix32 a, fix32 b) { return abs_fix(a - b) < fix32::from_float(0.015625); }

void test_carried_sideways() {
    Stage moving = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const Run r = go(moving, fix32{}, false, 30);
    std::printf("  sideways: char=%.3f plat=%.3f ground=%d\n", r.moved.x.to_double(),
                r.platform.x.to_double(), r.kept_ground ? 1 : 0);
    // Предпосылка: платформа реально уехала. Без неё «прошли одинаково» было бы правдой и про два
    // неподвижных тела.
    check(fx(55) < r.platform.x, "precondition: the platform actually travels");
    check(same(r.moved.x, r.platform.x), "a character on a moving platform travels with it");
    check(r.kept_ground, "and never loses the ground under him");

    // ПАРА: то же тело с той же ЗАПИСАННОЙ скоростью, но статическое. Мир его не двигает (статика
    // не интегрируется вовсе), значит и везти оно не вправе: перенос читает не поле, а движение.
    Stage plate = make_stage(physics::BodyType::Static, {CARRY_V, fix32{}}, false);
    const Run s = go(plate, fix32{}, false, 30);
    check(s.platform.x.raw == 0, "precondition: a static body does not move, whatever it says");
    check(s.moved.x.raw == 0, "pair: a written-but-unmoving velocity carries nobody");
}

void test_carried_up_and_down() {
    Stage rising = make_stage(physics::BodyType::Kinematic, {fix32{}, -RISE_V}, false);
    const Run up = go(rising, fix32{}, false, 30);
    std::printf("  up:   char=%.3f plat=%.3f ground=%d\n", up.moved.y.to_double(),
                up.platform.y.to_double(), up.kept_ground ? 1 : 0);
    check(up.platform.y < fx(-25), "precondition: the lift actually rises");
    check(same(up.moved.y, up.platform.y), "a rising platform lifts the character with it");
    check(up.kept_ground, "and he stays on it the whole way up");

    // Спуск быстрее ОКНА ПРИТЯЖЕНИЯ (10 юнитов за тик против восьми): иначе «поехал вниз вместе с
    // ней» доказывал бы лишь то, что притяжение к полу догоняет уехавшую опору каждый тик.
    Stage falling = make_stage(physics::BodyType::Kinematic, {fix32{}, FALL_V}, false);
    const Run down = go(falling, fix32{}, false, 30);
    std::printf("  down: char=%.3f plat=%.3f ground=%d\n", down.moved.y.to_double(),
                down.platform.y.to_double(), down.kept_ground ? 1 : 0);
    check(fx(250) < down.platform.y, "precondition: the lift descends deeper than the snap");
    check(same(down.moved.y, down.platform.y), "a descending platform takes the character down");
    check(down.kept_ground, "and he never falls behind it into the air");
}

void test_the_jump_inherits_the_horizontal() {
    Stage moving = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const Run mv = go(moving, fix32{}, true, 25);
    Stage still = make_stage(physics::BodyType::Kinematic, {fix32{}, fix32{}}, false);
    const Run sp = go(still, fix32{}, true, 25);
    std::printf("  jump: moving dx=%.3f peak=%.3f | still dx=%.3f peak=%.3f\n",
                mv.moved.x.to_double(), mv.peak.to_double(), sp.moved.x.to_double(),
                sp.peak.to_double());
    check(fx(60) < sp.peak, "precondition: the jump does leave the platform");
    // Одиннадцать юнитов складываются из двух слагаемых, и оба обязаны быть: два — перенос ТОГО
    // ЖЕ тика, в котором персонаж отталкивается (шаг 0 идёт до шага 4, и на нём он ещё стоит),
    // девять — унесённая горизонталь, гаснущая `air_decel` за восемь тиков (120, 105, … 15 юнит/с).
    // Полоса вокруг суммы, а не «больше нуля»: без наследования остались бы те самые два юнита
    // переноса, и порог «больше нуля» прошёл бы и на них.
    check(fx(10) < mv.moved.x && mv.moved.x < fx(12), "a jump off a moving platform keeps its speed");
    // ПАРА без единой правки, кроме скорости платформы: тот же прыжок с неподвижной уходит строго
    // вверх. Без неё утверждение выше доказывало бы лишь то, что персонаж куда-то смещается.
    check(sp.moved.x.raw == 0, "pair: the same jump off a still platform goes straight up");
}

void test_the_jump_does_not_inherit_the_vertical() {
    Stage rising = make_stage(physics::BodyType::Kinematic, {fix32{}, -RISE_V}, false);
    const Run up = go(rising, fix32{}, true, 25);
    Stage still = make_stage(physics::BodyType::Kinematic, {fix32{}, fix32{}}, false);
    const Run sp = go(still, fix32{}, true, 25);
    std::printf("  lift jump: peak=%.3f (a still platform gives %.3f)\n", up.peak.to_double(),
                sp.peak.to_double());
    check(up.platform.y < fx(-20), "precondition: the platform is rising under the jump");
    // Высота прыжка — обещание ПРОФИЛЯ (`jump_height`), и лифт не вправе его менять: унаследованная
    // вертикаль подняла бы вершину с 64 до 85, то есть на полтора тайла мимо того, куда целится
    // игрок. Сравнение с парой, а не с числом: разъехавшись, они назовут ЛИФТ, а не профиль.
    //
    // Один тик переноса вычитается, и это не подгонка: вершина мерится от старта прогона, а лифт
    // успевает поднять персонажа на `RISE_V * dt` ДО того, как тот оттолкнётся (шаг 0 идёт раньше
    // шага 4). Разница вершин обязана быть ровно этим тиком и ничем сверх него — наследование дало
    // бы двадцать один юнит, а не один.
    check(same(up.peak - RISE_V * tick_dt(), sp.peak),
          "a jump off a rising platform is as high as off a still one");
}

void test_the_crush_is_a_fact() {
    Stage lift = make_stage(physics::BodyType::Kinematic, {fix32{}, -CRUSH_V}, true);
    const Run r = go(lift, fix32{}, false, 12);
    std::printf("  crush: char=%.3f plat=%.3f crushed=%d\n", r.moved.y.to_double(),
                r.platform.y.to_double(), r.crushed ? 1 : 0);
    check(r.platform.y < fx(-40), "precondition: the lift keeps rising past the character");
    check(r.crushed, "a character carried into a ceiling is reported crushed");
    // Отказ АТОМАРЕН и не делает с персонажем ничего: над макушкой 19.875 юнита, шаг переноса — 4,
    // значит четыре переноса проходят (16), а пятый отбивается целиком. Полоса выбрана так, что
    // отбивает обе поломки разом: без переноса вышло бы 0, с переносом «сколько влезло» — 19.875,
    // то есть персонаж, вдавленный в потолок.
    check(fx(-18) < r.moved.y && r.moved.y < fx(-14), "and the refused carry moves him not at all");

    // ПАРА: тот же лифт с той же скоростью, но без потолка. Без неё «вжало» доказывало бы лишь то,
    // что флаг вообще умеет быть истинным.
    Stage open = make_stage(physics::BodyType::Kinematic, {fix32{}, -CRUSH_V}, false);
    const Run f = go(open, fix32{}, false, 12);
    check(!f.crushed, "pair: the same lift without a ceiling never reports it");
    check(same(f.moved.y, f.platform.y), "pair: and it carries him the whole way");
}

// Опора помнится через окно coyote и забывается вместе с ним. Прогоном по шагам, а не через `go`:
// вопрос здесь про ТИК, на котором меняется поле, и усреднённый путь про него не говорит ничего.
void test_the_support_is_forgotten() {
    Stage moving = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    Character c = rider_at(-PLATFORM_HALF_W);
    acquire_support(moving, p, d, c);

    bool mine_on_ground = true;
    bool in_window = false;
    bool after_window = true;
    int air = -1;
    for (uint32_t t = 0; t < 90; ++t) {
        moving.world.step(tick_dt());
        MoveInput in;
        in.move_x = fix32::from_int(1);
        step(moving.view(), make_rider(), p, d, in, tick_dt(), c);
        if (c.on_ground) {
            mine_on_ground = mine_on_ground && c.support.valid() &&
                             c.support.index == moving.platform.index;
            continue;
        }
        ++air;
        if (air == 0) in_window = c.support.valid();
        if (air == static_cast<int>(p.coyote_ticks)) after_window = c.support.valid();
    }
    std::printf("  forget: air ticks=%d window=%d after=%d\n", air, in_window ? 1 : 0,
                after_window ? 1 : 0);
    check(air >= static_cast<int>(p.coyote_ticks), "precondition: the walk leaves the platform");
    check(mine_on_ground, "the support is the very body he stands on");
    check(in_window, "it survives the tick he steps off: the coyote window still calls it his");
    check(!after_window, "and is forgotten together with the window");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character platform gate\n");
    test_carried_sideways();
    test_carried_up_and_down();
    test_the_jump_inherits_the_horizontal();
    test_the_jump_does_not_inherit_the_vertical();
    test_the_crush_is_a_fact();
    test_the_support_is_forgotten();
    std::printf("framework-character-platform: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
