#include <cstdio>

#include "controller.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
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
    test_counter_sees_a_real_allocation();
    std::printf("framework-character-alloc: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
