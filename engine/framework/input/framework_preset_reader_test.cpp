#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "preset_bake.hpp"
#include "preset_format.hpp"
#include "presets.hpp"

// Что ЧИТАТЕЛЬ секции обязан отбить. Отдельная цель от отказов бейка (`framework_preset_refusal_test`)
// по тому же основанию, по которому та отделена от round-trip: предмет здесь — байты секции, а не
// текст манифеста, и сломанная проверка даёт не отказ с номером строки, а порченый бандл, читаемый
// как чужая память. Каждая фикстура — испечённая таблица с ОДНОЙ правкой: секция, собранная руками
// целиком, сверялась бы сама с собой.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

// Неиспёкшаяся фикстура останавливает прогон: правки ниже пишут по смещениям из её заголовка, и в
// пустом буфере набор ронял бы сам себя порчей памяти, а не ловил чужую.
bool baked(const std::string& text, std::vector<uint8_t>& out) {
    framework::input::PresetBakeError err;
    if (framework::input::bake_presets(text, out, err)) return true;
    std::printf("  FAIL: a fixture did not bake: line %d: %s\n", err.line, err.message.c_str());
    return false;
}

void put32(std::vector<uint8_t>& b, std::size_t at, uint32_t v) {
    std::memcpy(b.data() + at, &v, sizeof(v));
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;

    std::vector<uint8_t> blob;
    if (!baked("preset | p\naxis | move_x | key:d | key:a\n", blob)) {
        std::printf("framework-preset-reader: FAIL\n");
        return 1;
    }

    PresetTable good;
    check(good.open(blob.data(), blob.size()), "the untouched table is the positive control");
    PresetTable bad;
    std::vector<uint8_t> broken;
    // Смещение таблицы проверяется снизу и на выравнивание, а не только на «влезает». Нулевое
    // кладёт строки поверх заголовка; нечётное даёт `reinterpret_cast` мимо границы. Фикстура
    // невыровненности собрана сдвигом СОДЕРЖИМОГО вместе со всеми смещениями заголовка — правка
    // одного поля отбивается проверкой размера, и утверждение вышло бы вакуумным.
    broken = blob;
    put32(broken, offsetof(PresetHeader, presets_offset), 0);
    check(!bad.open(broken.data(), broken.size()), "a table overlapping the header is rejected");
    broken = blob;
    broken.insert(broken.begin() + sizeof(PresetHeader), 2, 0);
    for (std::size_t at : {offsetof(PresetHeader, presets_offset),
                           offsetof(PresetHeader, actions_offset),
                           offsetof(PresetHeader, axes_offset),
                           offsetof(PresetHeader, bindings_offset),
                           offsetof(PresetHeader, pads_offset),
                           offsetof(PresetHeader, strings_offset),
                           offsetof(PresetHeader, total_size)}) {
        uint32_t v = 0;
        std::memcpy(&v, broken.data() + at, sizeof(v));
        put32(broken, at, v + 2);
    }
    check(!bad.open(broken.data(), broken.size()), "a misaligned table offset is rejected");

    const bool pass = (fails == 0);
    std::printf("framework-preset-reader: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
