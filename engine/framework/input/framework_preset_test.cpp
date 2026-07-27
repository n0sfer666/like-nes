#include <cstdio>
#include <string>
#include <vector>

#include "codes.hpp"
#include "platform_args.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"
#include "source_names.hpp"

// Гейт 1 спеки #14: одна и та же раскладка, объявленная текстом, обязана дать один и тот же
// InputFrame. Проверка сквозная — манифест → таблица → ActionMap → resolve, — потому что
// разойтись эти четыре шага могут только вместе: бейк, который печёт не то, и загрузчик, который
// читает не так, поодиночке выглядят исправными.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* MANIFEST = R"(
# раскладка по умолчанию
preset | default
action | jump   | key:space | pad:south
action | fire   | mouse:left | trigger:rt
action | pause  | key:esc | pad:start
axis   | move_x | key:d | key:a
axis   | move_x | key:right | key:left
axis   | move_x | padaxis:lx | -
axis   | move_y | key:w | key:s
axis   | look_x | padaxis:rx | -
shape  | move_x | 0.0 | 1.0 | 1 | move_y
shape  | move_y | 0.0 | 1.0 | 1 | move_x
shape  | look_x | 0.18 | 0.9 | 2 | -

preset | southpaw
action | jump   | pad:south
axis   | look_x | padaxis:lx | -
axis   | look_y | padaxis:-ly | -
shape  | look_x | 0.2 | 1.0 | 1 | look_y
shape  | look_y | 0.2 | 1.0 | 1 | look_x
)";

using framework::input::PresetTable;

std::string source_roundtrip(const char* text) {
    ::input::Source s;
    if (!framework::input::parse_source(text, s)) return "<parse failed>";
    return framework::input::source_name(s);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;

    // Имена источников — общий словарь бейка, перебиндов и UI: круг «текст → Source → текст»
    // обязан быть тождественным, иначе сохранённый перебинд прочитается не тем источником.
    check(source_roundtrip("key:space") == "key:space", "key round-trip");
    check(source_roundtrip("key:f5") == "key:f5", "function key round-trip");
    check(source_roundtrip("pad:south") == "pad:south", "pad button round-trip");
    check(source_roundtrip("padaxis:-ly") == "padaxis:-ly", "inverted pad axis keeps its sign");
    check(source_roundtrip("trigger:rt") == "trigger:rt", "trigger keeps its own prefix");
    check(source_roundtrip("mouseaxis:x") == "mouseaxis:x", "mouse axis round-trip");
    check(source_roundtrip("key:nosuchkey") == "<parse failed>", "an unknown key is rejected");
    check(source_roundtrip("space") == "<parse failed>", "a source without a prefix is rejected");

    std::vector<uint8_t> blob;
    PresetBakeError err;
    if (!bake_presets(MANIFEST, blob, err)) {
        std::printf("  FAIL: bake failed at line %d: %s\n", err.line, err.message.c_str());
        return 1;
    }

    PresetTable t;
    check(t.open(blob.data(), blob.size()), "the baked table opens");
    check(t.preset_count() == 2, "both presets are baked");
    const int def = t.find_preset("default");
    const int south = t.find_preset("southpaw");
    check(def == 0 && south == 1, "presets are found by name");
    check(t.find_preset("nope") == -1, "an unknown preset is not found");
    check(t.action_count(static_cast<uint32_t>(def)) == 3, "the default preset has three actions");
    check(t.axis_count(static_cast<uint32_t>(def)) == 3, "the default preset has three axes");

    // Индексы локальны для пресета: `jump` в обоих — нулевое действие, хотя во втором пресете
    // объявлено единственным. Иначе добавление действия в один пресет двигало бы биты в другом.
    check(t.find_action(static_cast<uint32_t>(def), "jump") == 0, "jump is action 0 in default");
    check(t.find_action(static_cast<uint32_t>(south), "jump") == 0, "jump is action 0 in southpaw");
    check(t.find_action(static_cast<uint32_t>(south), "fire") == -1,
          "southpaw does not inherit actions of the preset before it");
    check(t.find_axis(static_cast<uint32_t>(def), "look_x") == 2, "look_x is axis 2 in default");
    // Три строки `move_x` — три альтернативных биндинга ОДНОЙ оси, а не три оси: иначе стрелки
    // и стик заняли бы чужие слоты в InputFrame.
    check(t.find_axis(static_cast<uint32_t>(def), "move_y") == 1,
          "alternative bindings of an axis do not shift the axes after it");

    // Форма отклика доехала из текста в таблицу без потери разрядов.
    const StickShape look = t.axis_shape(static_cast<uint32_t>(def), 2);
    check(look.deadzone == fix32::from_float(0.18), "the deadzone survives the bake");
    check(look.outer == fix32::from_float(0.9), "the outer edge survives the bake");
    check(look.curve_exp == 2, "the curve exponent survives the bake");
    check(t.axis_pair(static_cast<uint32_t>(def), 0) == 1, "move_x is paired with move_y");
    check(t.axis_pair(static_cast<uint32_t>(def), 1) == 0, "the pair is symmetric");
    check(t.axis_pair(static_cast<uint32_t>(def), 2) == NO_PAIR, "look_x has no pair");
    // Индекс пары тоже локален: в непервом пресете глобальный номер оси уже не совпадает с
    // локальным, и именно здесь путаница между ними видна.
    check(t.axis_pair(static_cast<uint32_t>(south), 0) == 1 &&
              t.axis_pair(static_cast<uint32_t>(south), 1) == 0,
          "pairs in a later preset are local indices, not global ones");

    ::input::ActionMap map;
    check(t.bind(static_cast<uint32_t>(def), map), "the preset binds into the action map");
    ::input::PlayerAssign pa;
    pa.use_kbd_mouse = true;
    pa.pad_slot = 0;
    map.assign_player(0, pa);

    namespace c = ::input::code;
    const int A_JUMP = t.find_action(static_cast<uint32_t>(def), "jump");
    const int A_FIRE = t.find_action(static_cast<uint32_t>(def), "fire");
    const int AX_X = t.find_axis(static_cast<uint32_t>(def), "move_x");
    const int AX_Y = t.find_axis(static_cast<uint32_t>(def), "move_y");

    ::input::DeviceState d;
    d.pad_connected[0] = true;
    d.keys[c::Space >> 6] |= (1ull << (c::Space & 63));
    ::input::InputFrame f = map.resolve(d, 0, 0, 0);
    check(f.action_held(A_JUMP), "the keyboard binding of jump fires");
    check(f.action_pressed(A_JUMP), "the first tick of a held action is an edge");
    check(!f.action_held(A_FIRE), "an unbound source stays silent");

    // OR по источникам: то же действие с пада даёт тот же бит — ради этого действие и
    // device-agnostic.
    ::input::DeviceState pad;
    pad.pad_connected[0] = true;
    pad.pad_btns[0] |= (1u << c::PadA);
    check(map.resolve(pad, 0, 1, 0).action_held(A_JUMP), "the pad binding of the same action fires");

    // Клавиатурная пара как ось: направление и знак из манифеста, не из порядка полей.
    ::input::DeviceState keys;
    keys.keys[c::D >> 6] |= (1ull << (c::D & 63));
    f = map.resolve(keys, 0, 2, 0);
    check(f.axes[AX_X] == fix32::from_int(1), "the positive key drives the axis to +1");
    keys = ::input::DeviceState{};
    keys.keys[c::A >> 6] |= (1ull << (c::A & 63));
    f = map.resolve(keys, 0, 3, 0);
    check(f.axes[AX_X] == fix32::from_int(-1), "the negative key drives the axis to -1");
    check(f.axes[AX_Y] == fix32{}, "an untouched axis stays at zero");

    // Ошибки манифеста называют строку: без номера строки диагностика бейка бесполезна ровно
    // тогда, когда нужна.
    std::vector<uint8_t> ignored;
    check(!bake_presets("action | jump | key:space\n", ignored, err) && err.line == 1,
          "an action before any preset is refused with its line");
    check(!bake_presets("preset | p\naction | jump | key:nope\n", ignored, err) && err.line == 2,
          "an unknown source is refused with its line");
    check(!bake_presets("preset | p\naxis | x | key:d | key:a\nshape | x | 0.1 | 1.0 | 1 | y\n",
                        ignored, err) && err.line > 0,
          "a shape pairing an undeclared axis is refused");
    check(!bake_presets("# только комментарий\n", ignored, err), "an empty manifest is refused");
    check(!bake_presets("preset | p\naction | jump | key:space\naction | jump | pad:south\n",
                        ignored, err) && err.line == 3,
          "a second row for the same action is refused: its bindings must be contiguous");
    check(!bake_presets("preset | p\nwiggle | x\n", ignored, err) && err.line == 2,
          "an unknown row kind is refused with its line");

    // Битая таблица не открывается, а не читается как чужая память.
    std::vector<uint8_t> broken = blob;
    broken[0] = 'X';
    PresetTable bad;
    check(!bad.open(broken.data(), broken.size()), "a wrong magic is rejected");
    check(!bad.open(blob.data(), sizeof(PresetHeader) / 2), "a truncated blob is rejected");

    const bool pass = (fails == 0);
    std::printf("framework-preset: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
