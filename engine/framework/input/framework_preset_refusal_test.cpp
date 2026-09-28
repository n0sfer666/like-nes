#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "input_types.hpp"
#include "platform_args.hpp"
#include "preset_bake.hpp"
#include "preset_format.hpp"

// Что бейк обязан ОТБИТЬ — и что он обязан пропустить.
//
// Отдельная цель от `framework_preset_test`: там вопрос «доехало ли написанное до InputFrame», здесь
// — «названо ли ненаписуемое». Отказы падают иначе всех: сломанная проверка не даёт неверный кадр,
// она даёт манифест, испёкшийся МОЛЧА и разошедшийся с текстом, — потерянную форму, перевёрнутую ось
// или пресет, который загрузчик потом отвергает целиком. Имя упавшей цели в логе CI обязано отличать
// эти случаи. Отказы читателя — байты секции, а не текст — живут в `framework_preset_reader_test`.
//
// Каждый отказ проверяется вместе с НОМЕРОМ строки: без него диагностика бейка бесполезна ровно
// тогда, когда нужна. И каждый — вместе с позитивным контролем: проверка, отбивающая заодно честный
// манифест, вреднее отсутствующей, а три отказа отличаются от разрешённого написания одной деталью —
// направлением, порядком строк, единицей счёта.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

// Пределы движка проверяются манифестом, сгенерированным по числу, а не выписанным руками:
// шестьдесят пять строк в исходнике теста никто не пересчитает после правки MAX_ACTIONS.
std::string rows(const char* kind, const char* tail, int n) {
    std::string m = "preset | p\n";
    for (int i = 0; i < n; ++i) m += std::string(kind) + " | a" + std::to_string(i) + tail;
    return m;
}

// Одна ось строками-дубликатами: упирается в потолок строк, не задевая предел логических осей.
std::string one_axis(uint32_t n) {
    std::string m = "preset | p\n";
    for (uint32_t i = 0; i < n; ++i) m += "axis | x | key:d | key:a\n";
    return m;
}

// Пресеты по счёту: каждый занимает РОВНО две строки, поэтому номер отказа считается, а не пишется.
std::string many_presets(uint32_t n) {
    std::string m;
    for (uint32_t i = 0; i < n; ++i)
        m += "preset | p" + std::to_string(i) + "\naxis | x | key:d | key:a\n";
    return m;
}

// Пад с именами заданной длины: манифест обязан объявить пресет, иначе бейк откажет не за имя.
std::string pad_named(std::size_t name, std::size_t match) {
    return "preset | p\naxis | x | key:d | key:a\npad | " + std::string(name, 'n') +
           " | - | - | " + std::string(match, 'm') + " | xbox | 0.18 | 0.12\n";
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;

    std::vector<uint8_t> ignored;
    PresetBakeError err;

    // Грамматика строки: что вообще не является манифестом.
    check(!bake_presets("action | jump | key:space\n", ignored, err) && err.line == 1,
          "an action before any preset is refused with its line");
    check(!bake_presets("preset | p\naction | jump | key:nope\n", ignored, err) && err.line == 2,
          "an unknown source is refused with its line");
    check(!bake_presets("preset | p\naxis | x | key:d | key:a\nshape | x | 0.1 | 1.0 | 1 | y\n",
                        ignored, err) && err.line > 0,
          "a shape pairing an undeclared axis is refused");
    check(!bake_presets("# comment only\n", ignored, err), "an empty manifest is refused");
    check(!bake_presets("preset | p\naction | jump | key:space\naction | jump | pad:south\n",
                        ignored, err) && err.line == 3,
          "a second row for the same action is refused: its bindings must be contiguous");
    check(!bake_presets("preset | p\nwiggle | x\n", ignored, err) && err.line == 2,
          "an unknown row kind is refused with its line");

    // Строки ниже однажды пеклись МОЛЧА: одни давали раскладку, отличную от написанной, другие —
    // бандл, который потом отвергает читатель. Предел движка и потолки секции здесь та же
    // грамматика, только выраженная числом.
    check(!bake_presets("preset | p\naxis | move_x | key:d | key:a\n"
                        "shape | move_ex | 0.18 | 1.0 | 1 | -\n", ignored, err) && err.line == 3,
          "a shape naming an undeclared axis is refused, and by its own line");
    check(!bake_presets("preset | a\naxis | move_x | key:d | key:a\n"
                        "shape | look_x | 0.18 | 1.0 | 1 | -\n"
                        "preset | b\naxis | look_x | key:l | key:j\n", ignored, err) && err.line == 3,
          "a shape reaching for an axis of the NEXT preset is refused");
    check(!bake_presets("preset | p\naxis | move_x | key:d | key:a\n"
                        "shape | move_x | 0.1 | 1.0 | 1 | -\n"
                        "shape | move_x | 0.4 | 0.5 | 3 | -\n", ignored, err) && err.line == 4,
          "a second shape for one axis is refused instead of silently overwriting the first");
    check(!bake_presets("preset | p\naxis | move_x | key:d | key:a\n"
                        "axis | move_x | key:a | key:d\n", ignored, err) && err.line == 3,
          "an axis taking one source in both directions is refused: order would decide the sign");
    check(!bake_presets(rows("action", " | key:d\n", ::input::MAX_ACTIONS + 1), ignored, err) &&
              err.line == ::input::MAX_ACTIONS + 2,
          "a preset declaring more actions than the engine binds is refused at the bake");
    check(!bake_presets(rows("axis", " | key:d | key:a\n", ::input::MAX_AXES + 1), ignored, err) &&
              err.line == ::input::MAX_AXES + 2,
          "a preset declaring more axes than the engine binds is refused at the bake");
    check(!bake_presets(one_axis(MAX_AXIS_ROWS + 1), ignored, err) &&
              err.line == static_cast<int>(MAX_AXIS_ROWS) + 2,
          "a preset declaring more axis rows than the reader accepts is refused at the bake");
    check(!bake_presets("preset | " + std::string(MAX_NAME + 1, 'p') + "\n", ignored, err) &&
              err.line == 1,
          "a preset name longer than the reader's cap is refused at the bake");
    check(!bake_presets("preset | p\naction | " + std::string(MAX_NAME + 1, 'a') + " | key:d\n",
                        ignored, err) && err.line == 2,
          "an action name longer than the reader's cap is refused at the bake");
    check(!bake_presets("preset | p\naxis | " + std::string(MAX_NAME + 1, 'x') + " | key:d | key:a\n",
                        ignored, err) && err.line == 2,
          "an axis name longer than the reader's cap is refused at the bake");
    // Длина имени в сообщении: обрезок «первые 16 байт и многоточие» у длинных имён общий, и по
    // нему не понять ни какое имя виновато, ни насколько его резать (аудит #21, ревью A·2).
    check(err.message.find(std::to_string(MAX_NAME + 1)) != std::string::npos,
          "the refusal names the length of the offending name, not its first bytes");
    // Имена пада — те же имена блоба: и само имя профиля, и подстрока сопоставления.
    check(!bake_presets(pad_named(MAX_NAME + 1, 4), ignored, err) && err.line == 3,
          "a pad name longer than the reader's cap is refused at the bake");
    check(!bake_presets(pad_named(4, MAX_NAME + 1), ignored, err) && err.line == 3,
          "a pad name match longer than the reader's cap is refused at the bake");
    check(!bake_presets(many_presets(MAX_PRESETS + 1), ignored, err) &&
              err.line == static_cast<int>(MAX_PRESETS) * 2 + 1,
          "a manifest declaring more presets than the reader accepts is refused at the bake");
    // Ось, спаренная сама с собой: `pair` получил бы собственный номер оси, и проверка читателя
    // `pair < число логических осей` такую таблицу пропустила бы (аудит #21, ревью A·2).
    check(!bake_presets("preset | p\naxis | move_x | key:d | key:a\n"
                        "shape | move_x | 0.1 | 1.0 | 1 | move_x\n", ignored, err) && err.line == 3,
          "a shape pairing an axis with itself is refused: a radial zone needs two axes");

    // Позитивный контроль тех же отказов: отбивать ЧЕСТНЫЙ манифест они не должны, а часть из них
    // отличается от нарушения одной деталью — направлением, порядком строк, единицей счёта.
    check(bake_presets(rows("action", " | key:d\n", ::input::MAX_ACTIONS), ignored, err),
          "a preset filling the action capacity exactly still bakes");
    check(bake_presets(rows("axis", " | key:d | key:a\n", ::input::MAX_AXES), ignored, err),
          "a preset filling the axis capacity exactly still bakes");
    check(bake_presets(one_axis(MAX_AXIS_ROWS), ignored, err),
          "a preset filling the axis row capacity exactly still bakes");
    check(bake_presets("preset | " + std::string(MAX_NAME, 'p') + "\naction | " +
                           std::string(MAX_NAME, 'a') + " | key:d\naxis | " +
                           std::string(MAX_NAME, 'x') + " | key:d | key:a\n", ignored, err),
          "names filling the cap exactly still bake");
    check(bake_presets("preset | p\naxis | move_x | key:d | key:a\n"
                       "axis | move_x | key:d | key:a\n", ignored, err),
          "an exact duplicate axis row is not a contradiction: it names the same direction");
    check(bake_presets("preset | p\nshape | move_x | 0.1 | 1.0 | 1 | -\n"
                       "axis | move_x | key:d | key:a\n", ignored, err),
          "a shape written before its axis still attaches: shapes are applied at the preset's end");
    check(bake_presets(many_presets(MAX_PRESETS), ignored, err),
          "a manifest filling the preset capacity exactly still bakes");
    check(bake_presets(pad_named(MAX_NAME, MAX_NAME), ignored, err),
          "pad names filling the cap exactly still bake");
    check(bake_presets("preset | p\naxis | move_x | key:d | key:a\naxis | move_y | key:w | key:s\n"
                       "shape | move_x | 0.1 | 1.0 | 1 | move_y\n", ignored, err),
          "a shape pairing two DIFFERENT axes still bakes: only the self-pair is nonsense");

    const bool pass = (fails == 0);
    std::printf("framework-preset-refusal: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
