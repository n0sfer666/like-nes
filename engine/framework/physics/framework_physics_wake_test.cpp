#include <cstdio>

#include "platform_args.hpp"
#include "world.hpp"

// Три двери, через которые замерший остров обязан ожить, — и все три однажды были закрыты.
//
// Замирание останавливает остров целиком: ни тяготения, ни интеграции, ни решателя. Значит он не
// читает НИЧЕГО из поменявшегося вокруг, и закрытая дверь означает не «сработает позже», а не
// сработает никогда — будить его больше некому. Выглядит это не отказом, а сломанной физикой: ящик
// висит в воздухе там, где из-под него убрали пол. Проверяются ровно эти три:
//   1. смена тяготения           — и то, что ПОВТОРНАЯ выдача того же значения покой не отменяет;
//   2. сдвинутая статика         — пол уехал, стопка обязана упасть;
//   3. правка тела через ручку   — замечается СВЕРКОЙ, а не фактом выдачи ручки.
//
// Третий случай — единственный, где утверждение двустороннее, и обе стороны были дефектами: ответ на
// сам факт выдачи ручки выключает правило покоя молча, отсутствие ответа на правку молча её отменяет.
// Поэтому меряются оба конца — цена чтения (обязана быть нулевой) и доставка толчка НА ГРАНИЦЕ окна.
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

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
constexpr uint32_t BOXES = 3;
// Окно — граница отказа, а не ожидание: башня замирает за два десятка кадров, взято вдесятеро шире.
constexpr uint32_t WINDOW = 30 * 60;
constexpr uint32_t FLOOR = 0;
constexpr uint32_t TOP = BOXES;
constexpr fix32 GRIP = fix32::from_float(0.6);
// Толчок ВНУТРИ окна покоя: 62 raw за кадр против радиуса окна в 64, то есть тело за кадр из круга
// якоря не выходит, — ровно та величина, на которой правка терялась молча. Трение под ним обычное:
// без трения башня не замирает и за 30 секунд (измерено), а мерить доставку на незамёрзшей сцене
// значит мерить пустоту — гейт зеленел бы и с наглухо выключенным пробуждением.
constexpr int32_t CREEP_RAW = 62 * 60;
constexpr uint32_t SLIDE = 300;

void build(World& w, fix32 friction) {
    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(128), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = {fix32{}, friction};
    w.add(floor);

    for (uint32_t i = 0; i < BOXES; ++i) {
        BodyDesc b;
        b.key = 10 + i;
        b.shape = box(HALF, HALF);
        b.position = {fix32{}, FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16)};
        b.mass = fix32::from_int(4);
        b.material = {fix32{}, friction};
        w.add(b);
    }
}

bool tower_at_rest(const World& w) {
    for (uint32_t i = 1; i <= BOXES; ++i) {
        if (!w.at_rest(BodyId{i})) return false;
    }
    return true;
}

// Кадров до замирания башни, 0 — не замерла в окне.
uint32_t settle(World& w) {
    for (uint32_t i = 0; i < WINDOW; ++i) {
        w.step(DT);
        if (tower_at_rest(w)) return i + 1;
    }
    return 0;
}

fix32 top_y(const World& w) { return w.bodies()[TOP].position.y; }

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

void test_gravity_change_wakes() {
    World w{16};
    build(w, GRIP);
    check(settle(w) != 0, "a placed tower freezes, so there is something to wake");

    const fix32 before = top_y(w);
    // Вверх — так, чтобы ответ был однозначен по знаку: упасть обратно на пол ящик не может, а
    // «сдвинулся на пару raw» неотличимо от дрожания решателя.
    w.set_gravity({fix32{}, -w.gravity().y});
    check(!w.at_rest(BodyId{TOP}), "flipping gravity wakes the frozen island at once");
    for (uint32_t i = 0; i < 60; ++i) w.step(DT);
    check(top_y(w) < before, "and the tower actually leaves the floor it was resting on");
    std::printf("  gravity flipped: y %d -> %d raw\n", before.raw, top_y(w).raw);
}

// Обратная половина того же: тяготение, выданное повторно тем же значением, покой отменять НЕ вправе.
// Игра, читающая его из конфигурации каждый кадр, иначе выключила бы правило покоя целиком.
void test_same_gravity_keeps_rest() {
    World w{16};
    build(w, GRIP);
    for (uint32_t i = 0; i < WINDOW; ++i) {
        w.set_gravity(w.gravity());
        w.step(DT);
        if (tower_at_rest(w)) {
            std::printf("  re-set gravity every frame: froze at %u\n", i + 1);
            return;
        }
    }
    check(false, "re-setting the same gravity every frame does not keep the tower awake");
}

void test_moved_static_wakes() {
    World w{16};
    build(w, GRIP);
    check(settle(w) != 0, "the tower freezes before the floor is touched");

    const fix32 before = top_y(w);
    w.body(BodyId{FLOOR}).position.y = fix32::from_int(400);
    w.step(DT);
    check(!w.at_rest(BodyId{TOP}), "teleporting the floor wakes the island that stood on it");
    for (uint32_t i = 0; i < 60; ++i) w.step(DT);
    check(before < top_y(w), "and the tower falls after the floor that left");
    std::printf("  floor teleported: y %d -> %d raw\n", before.raw, top_y(w).raw);
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
    std::printf("framework physics wake gate\n");
    test_gravity_change_wakes();
    test_same_gravity_keeps_rest();
    test_moved_static_wakes();
    test_reading_a_handle_costs_nothing();
    test_writing_through_a_handle_lands();
    std::printf("framework-physics-wake: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
