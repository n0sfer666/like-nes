#include <cstdio>

#include "framework_physics_wake_scene.hpp"
#include "platform_args.hpp"

// Третья дверь пробуждения — правка тела через неконстантную ручку — и единственная, где утверждение
// двустороннее. Обе стороны были дефектами: ответ на сам факт выдачи ручки выключает правило покоя
// молча, отсутствие ответа на правку молча её отменяет. Поэтому меряются оба конца — цена чтения
// (обязана быть нулевой) и доставка толчка НА ГРАНИЦЕ окна покоя.
//
// Отдельной целью от `framework_physics_wake_test`: две первые двери отвечают «открылась или нет»,
// эта — двумя числами, и оба порога взяты из устройства правила покоя, а не из прогона. Имя упавшей
// цели в логе CI обязано отличать эти случаи. Сцена — общая, `framework_physics_wake_scene.hpp`.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::physics;
using namespace framework::physics::wake;

// Толчок ВНУТРИ окна покоя: 62 raw за кадр против радиуса окна в 64, то есть тело за кадр из круга
// якоря не выходит, — ровно та величина, на которой правка терялась молча.
constexpr int32_t CREEP_RAW = 62 * 60;
constexpr uint32_t SLIDE = 300;

// Сколько верхний ящик проедет от толчка `CREEP_RAW` через ручку; `explicit_wake` выбирает путь —
// явное пробуждение против одной лишь записи. Оба обязаны дать ОДНО И ТО ЖЕ число: «толкнули» не
// бывает слабее оттого, что забыли сказать про это вслух.
fix32 creep(bool explicit_wake) {
    World w{16};
    build(w, GRIP);
    check(settle(w) != 0, "the tower the creeping push is measured on really freezes first");
    const BodyId top{TOP};
    const fix32 before = w.bodies()[TOP].position.x;
    if (explicit_wake) w.wake(top);
    w.body(top).velocity.x = fix32::from_raw(CREEP_RAW);
    for (uint32_t i = 0; i < SLIDE; ++i) w.step(DT);
    return w.bodies()[TOP].position.x - before;
}

// Чтение через неконстантную ручку обязано стоить РОВНО НОЛЬ. Сверяются два одинаковых мира, во
// втором каждый кадр берётся ручка каждого тела — то, что делает цикл отрисовки. Утверждение по
// ХЕШУ, а не по `at_rest`: разморозка «на кадр» покой сохраняла (`at_rest` всё время `true`), а
// траекторию сдвигала — отпущенный остров проживает кадр целиком и замерзает в другой точке.
void test_reading_a_handle_costs_nothing() {
    World quiet{16};
    World watched{16};
    build(quiet, GRIP);
    build(watched, GRIP);
    const uint32_t froze = settle(quiet);
    check(froze != 0, "the tower freezes in the run nobody touches");
    check(settle(watched) == froze, "and in its twin at the very same frame");

    uint32_t diverged = 0;
    for (uint32_t i = 0; i < 600; ++i) {
        for (uint32_t b = 0; b <= BOXES; ++b) (void)watched.body(BodyId{b});
        quiet.step(DT);
        watched.step(DT);
        if (diverged == 0 && quiet.hash() != watched.hash()) diverged = i + 1;
    }
    check(diverged == 0, "taking a non-const handle every frame changes nothing in the state");
    if (diverged != 0) std::printf("  diverged at frame %u\n", diverged);
    check(tower_at_rest(watched), "and the run that was read from stays frozen throughout");
}

// А запись через ту же ручку обязана доехать, и доехать ЦЕЛИКОМ. Быстрый толчок выносит тело из круга
// якоря сам и доедет при любой реализации, медленный — только если правку заметили. Равенство с путём
// через `wake()` заодно пинит отсутствие потерянного кадра: проспи мир шаг, и путь станет короче.
void test_writing_through_a_handle_lands() {
    const fix32 by_hand = creep(false);
    const fix32 by_wake = creep(true);
    std::printf("  crept %d raw through a handle, %d after an explicit wake()\n", by_hand.raw,
                by_wake.raw);
    check(by_hand == by_wake, "a push through a handle travels exactly as far as after wake()");
    // Спрашивается ЗНАК, а не дальность. Незамеченная правка даёт РОВНО ноль: замерший остров шаг не
    // интегрирует, а `settle` того же кадра обнуляет ему скорость, — так что ноль отделяет отмену от
    // доставки без всякого порога. Дальность же под трением задаёт решатель, и пинить её значило бы
    // краснеть от каждой его правки.
    check(by_hand.raw > 0, "and it is not cancelled by the very step it was handed to");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics handle gate\n");
    test_reading_a_handle_costs_nothing();
    test_writing_through_a_handle_lands();
    std::printf("framework-physics-handle: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
