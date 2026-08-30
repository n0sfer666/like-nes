#include <cstdio>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "nine_slice.hpp"
#include "platform_args.hpp"
#include "sprite.hpp"

// Гейт 8 спеки #17: установившийся кадр не ходит в кучу.
//
// Требование это не абстрактное. Игра-образец держит инстансы в `std::vector` (`SpriteBatch` в
// `example_ugly_game/batch.hpp`), то есть в вырожденном случае оно нарушено прямо сейчас, и шаг A
// обобщает раскладку ровно с обещанием, что буферы принадлежат вызывающему. Обещание, которое
// никто не считает, ломается молча: `std::stable_sort` выделяет буфер под слияние, и подмена им
// `std::sort` «ради стабильности» выглядела бы как безобидная правка одной строки.
//
// Счётчик и его позитивный контроль ОБЩИЕ с гейтами персонажа и физики
// (`framework_alloc_probe.hpp`, `framework_alloc_probe_control.hpp`): две копии «нуля аллокаций»,
// считающие разное под одним именем, хуже одной.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::graphics;

constexpr uint32_t CAP = 2048;
Sprite storage[CAP];
uint64_t keys[CAP];
Batch batches[CAP];

Sprite at(uint32_t i, uint32_t frame) {
    Sprite s;
    s.center = {fix32::from_int(static_cast<int32_t>(i % 40)),
                fix32::from_int(static_cast<int32_t>((i + frame) % 30))};
    s.half = {fix32::from_int(4), fix32::from_int(4)};
    s.rgba = 0xffffffffu;
    s.region = static_cast<RegionId>(i % 17);
    s.material = static_cast<uint16_t>((i * 7 + frame) % 5);
    s.layer = static_cast<int16_t>(static_cast<int>(i % 5) - 2);
    return s;
}

// Кадр обязан пройти ВЕСЬ путь раскладки: подача, 9-slice поверх неё, сортировка и нарезка на
// батчи. Счётчик на одной только сортировке отвечал бы про `std::sort`, а не про кадр.
uint32_t frame(SpriteList& list, uint32_t f) {
    list.clear();
    for (uint32_t i = 0; i < 1024; ++i) list.push(at(i, f));
    nine_slice(list, NineSliceRegions{1, 2, 3, 4, 5, 6, 7, 8, 9},
               {fix32::from_int(100), fix32::from_int(60)},
               {fix32::from_int(40), fix32::from_int(25)},
               {fix32::from_int(6), fix32::from_int(6)}, 0xffffffffu, 4,
               static_cast<int16_t>(f % 3));
    return list.build(batches, CAP);
}

void test_frame_allocates_nothing() {
    SpriteList list(storage, keys, CAP);
    // Первый кадр прогоняется ДО счётчика: кучу мог бы тронуть не он, а ленивая инициализация чего
    // угодно под ним, и обвинён был бы кадр.
    const uint32_t first = frame(list, 0);

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    uint32_t total = 0;
    for (uint32_t f = 1; f < 120; ++f) total += frame(list, f);
    const long during = framework::probe::allocs;
    framework::probe::in_hot = false;

    std::printf("  allocs: 119 frames = %ld, batches %u (first frame %u)\n", during, total, first);
    check(during == 0, "an established frame allocates nothing");
    // Прогон обязан состояться: кадр, в котором ноль спрайтов и ноль батчей, тоже не ходит в кучу.
    // Порогом, а не нулём, по тому же основанию, что у лестницы в гейте персонажа.
    check(total > 119, "control: the counted frames really did lay sprites out");
    check(list.dropped() == 0, "control: nothing was silently dropped instead of laid out");
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
    platform::Args utf8_argv(argc, argv);
    std::printf("sprite layout allocates nothing\n");

    test_counter_sees_a_real_allocation();
    test_frame_allocates_nothing();

    std::printf("framework-graphics-batch-alloc: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
