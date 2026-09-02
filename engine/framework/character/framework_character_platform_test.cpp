#include <cstdio>

#include "framework_character_platform_scene.hpp"
#include "platform_args.hpp"
#include "support.hpp"

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

// Пассажир, которому перенос упирается ВБОК, скользит по опоре, а не давится. Третья находка
// владельческого прогона §6 (2026-09-01, уже по фиксам первых двух): «сбрасывает в спавн» — плита
// везла персонажа, прижатого к козырьку, отказ переноса поднимал `crushed`, и образец отвечал на
// флаг возвратом в точку появления.
//
// Атомарность переноса тут ни при чём, и это разные вопросы. Вверх платформа персонажа ПРИЖИМАЕТ к
// потолку: деваться некуда, и отказ — единственный честный ответ (`test_the_crush_is_a_fact`).
// Вбок она лишь ЕДЕТ у него под ногами: сверху ничего нет, стена рядом не давит, и остановленный
// ею перенос означает, что крыша проехала под подошвами, — то есть ровно то, что делает ящик на
// ленте, упёршийся в стену.
void test_a_blocked_ride_is_a_slip() {
    Stage moving = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    add_pillar(moving);
    const Run r = go(moving, fix32{}, false, 20);
    std::printf("  slip: char=%.3f plat=%.3f ground=%d crushed=%d\n", r.moved.x.to_double(),
                r.platform.x.to_double(), r.kept_ground ? 1 : 0, r.crushed ? 1 : 0);
    check(fx(35) < r.platform.x, "precondition: the platform travels the whole way");
    check(r.moved.x < r.platform.x, "precondition: the pillar really holds the rider back");
    check(!r.crushed, "a rider held by a wall slips on his platform instead of being crushed");
    check(r.kept_ground, "and keeps standing on it");
    // Четырнадцать — последний ЦЕЛЫЙ шаг переноса перед касанием: правый бок пассажира встаёт под
    // столб на 16, шаг переноса — два юнита. Число, а не «меньше платформы»: перенос «сколько
    // влезло» дал бы 15.9375, то есть пассажира, вжатого в столб по зазор касания.
    check(same(r.moved.x, fx(14)), "and stops a whole carry step short of it");

    // ПАРА: та же плита без столба увозит его целиком. Без неё «скользит» проходило бы и у
    // реализации, разучившейся возить вбок вовсе.
    Stage open = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const Run f = go(open, fix32{}, false, 20);
    check(!f.crushed && same(f.moved.x, f.platform.x), "pair: without the pillar it carries him all the way");

    // Скольжение отбирает у переноса ровно ту составляющую, которой некуда деться. Плита, едущая
    // ВБОК И ВНИЗ, обязана опустить прижатого пассажира на весь свой путь по вертикали: отброшенный
    // целиком перенос оставил бы его висеть, и «не раздавило» было бы правдой про персонажа,
    // которого платформа больше не везёт вовсе.
    Stage sinking = make_stage(physics::BodyType::Kinematic, {CARRY_V, RISE_V}, false);
    add_pillar(sinking);
    const Run s = go(sinking, fix32{}, false, 20);
    std::printf("  slip down: char=(%.3f, %.3f) plat=(%.3f, %.3f)\n", s.moved.x.to_double(),
                s.moved.y.to_double(), s.platform.x.to_double(), s.platform.y.to_double());
    check(fx(15) < s.platform.y, "precondition: the sinking platform really descends");
    check(!s.crushed && s.moved.x < s.platform.x, "the held rider slips sideways there too");
    check(same(s.moved.y, s.platform.y), "and is still carried down the whole way");
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
// Опора — ЗАПОМНЕННОЕ состояние, а сцена приезжает аргументом: связывает их только вызывающий, и
// перезагруженный уровень даёт ДРУГОЙ вектор тел. Индекс, стабильный в своём мире, в чужом
// указывает за конец, а `World::body()` индексирует без проверки границ. Здесь это утверждение о
// НЕЧТЕНИИ за концом, поэтому судит его не только полоса ниже: гейт входит в санитайзерный цикл CI,
// и без клампа тот же прогон под ASan падает на чтении, а не на несовпавшем числе.
void test_the_support_belongs_to_its_world() {
    Stage st = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const physics::BodyId far_away = st.platform;
    physics::World empty(1);
    const CollisionScene other = {&empty, nullptr};
    const Vec2 v = support_velocity(other, far_away);
    std::printf("  foreign world: bodies=%zu index=%u velocity=(%.3f, %.3f)\n",
                empty.bodies().size(), far_away.index, v.x.to_double(), v.y.to_double());
    check(empty.bodies().size() <= far_away.index, "precondition: the index is past that world's end");
    check(v.x.raw == 0 && v.y.raw == 0, "a support index out of the scene's world carries nobody");
    // Пара: та же опора в СВОЁМ мире по-прежнему везёт. Иначе «ноль» выше проходило бы и у
    // реализации, разучившейся читать скорость вовсе.
    const Vec2 mine = support_velocity(st.view(), st.platform);
    check(mine.x == CARRY_V, "pair: and the same one in its own world still does");
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
    test_a_blocked_ride_is_a_slip();
    test_the_support_is_forgotten();
    test_the_support_belongs_to_its_world();
    std::printf("framework-character-platform: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
