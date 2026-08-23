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
//
// Возвращаются ДВЕ величины, и это разделение вынуждено замером. `first` — путь за ПЕРВЫЙ шаг после
// записи, то есть сам факт доставки. `total` — путь за все `SLIDE` кадров, и он о доставке уже не
// говорит: трение под толчком обычное, оно гасит его за два кадра, а оставшиеся под три сотни ящик
// живёт остатком решателя. Прежняя редакция спрашивала знак у `total`, и подъём числа итераций с 8
// до 16 (решатель стал гасить быстрее) увёл его с +28 в −23 при неизменившейся доставке: первый шаг
// как давал +27, так и даёт. То есть гейт краснел не от потерянной правки, а от того, что мерил
// сходимость решателя, называя её доставкой.
struct Creep {
    fix32 first;
    fix32 total;
};

Creep creep(bool explicit_wake) {
    World w{16};
    build(w, GRIP);
    check(settle(w) != 0, "the tower the creeping push is measured on really freezes first");
    const BodyId top{TOP};
    const fix32 before = w.bodies()[TOP].position.x;
    if (explicit_wake) w.wake(top);
    w.mutate(top).velocity.x = fix32::from_raw(CREEP_RAW);
    w.step(DT);
    const fix32 first = w.bodies()[TOP].position.x - before;
    for (uint32_t i = 1; i < SLIDE; ++i) w.step(DT);
    return {first, w.bodies()[TOP].position.x - before};
}

// ВЫДАЧА ручки правки обязана стоить РОВНО НОЛЬ. Сверяются два одинаковых мира, во втором каждый
// кадр берётся ручка каждого тела и НИЧЕГО ею не пишется. Утверждение по ХЕШУ, а не по `at_rest`:
// разморозка «на кадр» покой сохраняла (`at_rest` всё время `true`), а траекторию сдвигала —
// отпущенный остров проживает кадр целиком и замерзает в другой точке.
//
// Спрашивается именно `mutate`, а не чтение: чтение с этого раунда физически не может ничего
// сделать — `body()` остался только константным, и утверждение о нём стало бы вакуумным. Живым
// остался тот же вопрос по другую сторону шва: `mutate` метит индекс запросов протухшим и обязан
// не делать БОЛЬШЕ ничего. Допиши в него `rest_.wake_all()` — окно покоя не досчитает до
// `REST_FRAMES` ни разу, и этот гейт покраснеет.
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
        for (uint32_t b = 0; b <= BOXES; ++b) (void)watched.mutate(BodyId{b});
        quiet.step(DT);
        watched.step(DT);
        if (diverged == 0 && quiet.hash() != watched.hash()) diverged = i + 1;
    }
    check(diverged == 0, "taking the mutate handle every frame changes nothing in the state");
    if (diverged != 0) std::printf("  diverged at frame %u\n", diverged);
    check(tower_at_rest(watched), "and the run whose handles were taken stays frozen throughout");
}

// А запись через ту же ручку обязана доехать, и доехать ЦЕЛИКОМ. Быстрый толчок выносит тело из круга
// якоря сам и доедет при любой реализации, медленный — только если правку заметили. Равенство с путём
// через `wake()` заодно пинит отсутствие потерянного кадра: проспи мир шаг, и путь станет короче.
void test_writing_through_a_handle_lands() {
    const Creep by_hand = creep(false);
    const Creep by_wake = creep(true);
    std::printf("  crept %d raw on the first step, %d over %u frames (%d and %d after wake())\n",
                by_hand.first.raw, by_hand.total.raw, SLIDE, by_wake.first.raw, by_wake.total.raw);
    // Равенство спрашивается у ОБЕИХ величин и у полной дистанции в том числе: потерянный кадр
    // разошёлся бы и на первом шаге, а разъехавшийся дальше по прогону — только на полной.
    check(by_hand.first == by_wake.first, "a push through a handle lands exactly as after wake()");
    check(by_hand.total == by_wake.total, "and travels exactly as far as after wake()");
    // Спрашивается ЗНАК, а не дальность, и спрашивается он у ПЕРВОГО шага. Незамеченная правка даёт
    // на нём РОВНО ноль: замерший остров шаг не интегрирует, а `settle` того же кадра обнуляет ему
    // скорость, — так что ноль отделяет отмену от доставки без всякого порога. Дальше первого шага
    // ходом распоряжается уже трение, а не тот, кто толкал: знак суммы за три сотни кадров — это
    // знак остатка решателя, и пинить его значило бы краснеть от каждой его правки.
    check(by_hand.first.raw > 0, "and it is not cancelled by the very step it was handed to");
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
