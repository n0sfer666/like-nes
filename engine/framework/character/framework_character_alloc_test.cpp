#include <cstdio>

#include "controller.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "framework_character_ladder_scene.hpp"
#include "framework_character_platform_scene.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"

// Инвариант 5 спеки #16: тик персонажа не ходит в кучу.
//
// До этого гейта утверждение жило КОММЕНТАРИЕМ в `controller.hpp`, и это ровно тот случай, против
// которого заведён `vacuous-gate`: «zero-alloc устройством, а не дисциплиной» проверялось чтением
// кода, а `slide.hpp` при этом тянет `query.hpp`, где `overlap_shape` держит `std::vector`. Один
// вызов не тем запросом — и заявление стало бы неверным молча, потому что проверять его нечем.
//
// Счётчик общий с гейтом 6 спеки #15 (`framework_alloc_probe.hpp`): две копии «нуля аллокаций»,
// считающие разное под одним именем, хуже одной. Позитивный контроль тоже общий и обязателен —
// неработающий перехват выглядит как самый зелёный гейт на свете.
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

// Ввод обязан провести персонажа по ВСЕМ путям тика: бег, прыжок, удар о потолок, скольжение вдоль
// стены, сход с края и падение. Гейт на персонаже, простоявшем триста тиков на месте, проверял бы,
// что аллокаций нет у пробы опоры, — а не у тика.
MoveInput input_at(uint32_t t) {
    MoveInput in;
    in.move_x = t < 120 ? fix32::from_int(-1) : fix32::from_int(1);
    in.jump_held = (t % 37) < 6;
    return in;
}

void test_step_allocates_nothing() {
    Scene sc = make_scene();
    const CollisionScene s = sc.view();
    const CharacterHull hull = make_hull();
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    Character c = standing_at(fix32::from_int(-100));
    // Первый тик прогоняется ДО счётчика: кучу мог бы тронуть не он, а ленивая инициализация чего
    // угодно под ним, и обвинён был бы тик.
    step(s, hull, p, d, input_at(0), tick_dt(), c);

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (uint32_t t = 1; t < 300; ++t) step(s, hull, p, d, input_at(t), tick_dt(), c);
    const long during = framework::probe::allocs;
    framework::probe::in_hot = false;

    std::printf("  allocs: 299 ticks = %ld, ended at (%.2f, %.2f)\n", during, c.position.x.to_double(),
                c.position.y.to_double());
    check(during == 0, "step() allocates nothing");
    // Прогон обязан состояться: тик, ни разу не тронувший ни стену, ни потолок, ни воздух, отвечает
    // про куда более узкий путь, чем заявлено.
    check(c.position.x != fix32::from_int(-100), "control: the run really did move the character");
}

// Лестница — АЛЬТЕРНАТИВНЫЙ тик (вертикаль 3, шаг D), и общей сценой он не проверяется вовсе: в ней
// нет ни одного тайла лестницы, поэтому `ladder_step` там возвращает `false` первой же строкой. А
// ходит он в сетку СВОИМИ запросами — окном по вырожденному AABB и пробой опоры по сцене с
// дописанным исключением, — и одна проверка не тем запросом (`overlap_shape` держит `std::vector`)
// уронила бы инвариант 5 в режиме, которого счётчик не видит.
void test_ladder_step_allocates_nothing() {
    Sim sim(LADDER, 8, fix32::from_int(40));
    settle(sim);
    // Тот же порядок, что выше: первый тик режима — до счётчика.
    sim.run(held(true, false, false), 1);
    check(sim.c.state == MoveState::Ladder, "control: the ladder run really is on the ladder");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    // Ввод обязан пройти ВСЕ пути режима: лазание вверх, лазание вниз, висение без ввода, прыжок с
    // лестницы и перехват её обратно по истечении окна.
    uint32_t on_ladder = 0;
    for (uint32_t t = 0; t < 240; ++t) {
        const uint32_t phase = t % 60;
        sim.run(held(phase < 10, phase >= 10 && phase < 40, phase >= 50), 1);
        if (sim.c.state == MoveState::Ladder) ++on_ladder;
    }
    const long during = framework::probe::allocs;
    framework::probe::in_hot = false;

    std::printf("  allocs: 240 ladder ticks = %ld, on the ladder %u, ended at (%.2f, %.2f)\n",
                during, on_ladder, sim.c.position.x.to_double(), sim.c.position.y.to_double());
    check(during == 0, "the ladder tick allocates nothing");
    // Порогом, а не нулём: прогон, съехавший в обычный тик после первого же прыжка, отвечал бы про
    // контроллер под именем лестницы — и был бы зелёным, потому что про контроллер это уже правда.
    check(on_ladder > 120, "control: most of the counted ticks really were ladder ticks");
}

// Снос движущимся телом (шаг 0 тика, `push.hpp`) — третий путь, которого не видит ни один случай
// выше: в общей сцене тел нет вовсе, поэтому обход касаний там не зовётся ни разу. Дорога к куче тут
// самая короткая из всех — первый напрашивающийся запрос «кто здесь сейчас» (`overlap_shape`)
// отвечает ВЕКТОРОМ, и снос обязан спрашивать мир не им, а обходом.
void test_shove_allocates_nothing() {
    Stage st = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    MoveProfile p = default_profile();
    // Без тяготения: падающий персонаж выпал бы из полосы платформы раньше, чем та до него доедет,
    // и счётчик считал бы тики без единого касания.
    p.gravity_rise = fix32{};
    p.gravity_fall = fix32{};
    const MoveDerived d = derive(p, tick_dt());
    Character c;
    c.position = {HANG_X, HANG_Y};
    const fix32 from_x = c.position.x;
    st.world.step(tick_dt());
    step(st.view(), make_rider(), p, d, MoveInput{}, tick_dt(), c);

    // Счётчик включается вокруг ТИКА, а не вокруг кадра: шаг мира — чужая работа, под инвариант 5 он
    // не подпадает, и посчитанный вместе с тиком обвинял бы тик в чужой аллокации.
    long during = 0;
    for (uint32_t t = 0; t < 30; ++t) {
        st.world.step(tick_dt());
        framework::probe::in_hot = true;
        framework::probe::allocs = 0;
        step(st.view(), make_rider(), p, d, MoveInput{}, tick_dt(), c);
        during += framework::probe::allocs;
        framework::probe::in_hot = false;
    }

    std::printf("  allocs: 30 shoved ticks = %ld, moved %.2f\n", during,
                (c.position.x - from_x).to_double());
    check(during == 0, "the shoved tick allocates nothing");
    // Прогон обязан состояться: платформа, не доехавшая до персонажа, отвечает про тик, в котором
    // обход касаний не нашёл никого, — то есть про самый дешёвый путь из всех.
    check(from_x < c.position.x, "control: the platform really did shove the character");
}

void test_counter_sees_a_real_allocation() {
    // Выделение зовётся через НЕПРОЗРАЧНУЮ ГРАНИЦУ (`framework_alloc_probe_control.hpp`): в своём
    // TU компилятор выбрасывает пару new/delete целиком, и контроль краснел бы при живом счётчике.
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool plain_ok = framework::probe::control::plain_allocation();
    const long plain = framework::probe::allocs;
    framework::probe::allocs = 0;
    const bool aligned_ok = framework::probe::control::aligned_allocation();
    const long aligned = framework::probe::allocs;
    framework::probe::in_hot = false;
    std::printf("  control: plain=%ld aligned=%ld\n", plain, aligned);
    check(plain > 0, "control: the counter sees a real allocation");
    check(plain_ok, "control: that allocation really was handed out and written to");
    check(aligned > 0, "control: the counter sees an over-aligned allocation");
    check(aligned_ok, "control: the over-aligned allocation really is aligned");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character alloc gate\n");
    test_step_allocates_nothing();
    test_ladder_step_allocates_nothing();
    test_shove_allocates_nothing();
    test_counter_sees_a_real_allocation();
    std::printf("framework-character-alloc: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
