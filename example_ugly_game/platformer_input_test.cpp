#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "platformer_input.hpp"
#include "preset_bake.hpp"

// Гейт шва ввода живой цели (шаг C вертикали 3 спеки #16): раскладка образца обязана давать
// платформеру ТО намерение, которого он ждёт.
//
// Проверять тут есть что ровно из-за двух решений этого раунда. Первое — прыжок отдельным
// действием `jump`, а не переиспользованным `fire`: без утверждения ниже строка в манифесте
// исчезает молча, и прыжок уезжает на индекс -1. Второе — знак `move_y`: ось объявлена
// положительной ВВЕРХ, мир считает +Y вниз, и переворот знака здесь единственный. Оба — про
// ДАННЫЕ и про границу, поэтому ни контроллер, ни sim-голден про них не утверждают ничего:
// голден кормится таблицей тиков, а не кадром ввода.
namespace {

namespace pv = platformer;
namespace fi = framework::input;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_MANIFEST = "example_ugly_game/assets/input.txt";

// Кадр ввода собирается руками, а не бэкендом: вопрос здесь про ПЕРЕВОД кадра в намерение, и
// живой источник добавил бы к нему клавиатуру, окно и ОС — то есть всё, чего в вопросе нет.
::input::InputFrame frame_of(const pv::Binding& b, fix32 x, fix32 y, bool jump) {
    ::input::InputFrame f;
    f.axes[b.move_x] = x;
    f.axes[b.move_y] = y;
    if (jump) f.held |= 1ull << b.jump;
    return f;
}

void check_binding(const fi::PresetTable& table) {
    pv::Binding b;
    check(pv::resolve_binding(table, "default", b), "the shipped preset binds jump, move_x, move_y");
    if (!b.valid()) return;

    // Прыжок — СВОЁ действие. Утверждение стоит именно так, а не «индекс равен 1»: индекс есть
    // порядок строк, и он законно двигается, а вот совпадение прыжка со стрельбой означало бы,
    // что строку `jump` из манифеста убрали, а игра этого не заметила.
    const int preset = table.find_preset("default");
    const uint32_t p = static_cast<uint32_t>(preset);
    const int fire = table.find_action(p, "fire");
    check(fire >= 0 && fire != b.jump, "jump is an action of its own, not the shooter's fire");
    check(b.move_x != b.move_y, "the two axes are two axes");

    pv::Binding missing;
    check(!pv::resolve_binding(table, "no-such-preset", missing),
          "a preset that is not there is refused, not answered with -1");

    const pv::ch::MoveInput idle = pv::read_input(::input::InputFrame{}, missing);
    check(idle.move_x.raw == 0 && !idle.jump_held && !idle.down_held,
          "an unresolved binding reads as no input at all, never as someone else's axis");
}

// Раскладка, в которой пресет ЕСТЬ, а `jump` в нём нет. Отдельным манифестом, а не правкой
// игрового: вопрос здесь про отказ шва, и проверить его на файле, где строка прыжка стоит,
// нечем — `resolve_binding`, возвращающий «да» безусловно, проходит такой прогон целиком.
// Молчаливое «да» тут стоит дорого: `read_input` пойдёт читать ось по индексу -1.
const char* LAME_MANIFEST =
    "preset | lame\n"
    "action | fire   | key:space\n"
    "axis   | move_x | key:d | key:a\n"
    "axis   | move_y | key:w | key:s\n";

void check_incomplete() {
    std::vector<uint8_t> blob;
    fi::PresetBakeError err;
    if (!fi::bake_presets(LAME_MANIFEST, blob, err)) {
        check(false, "the cut-down layout bakes at all");
        return;
    }
    fi::PresetTable table;
    if (!table.open(blob.data(), blob.size())) {
        check(false, "the cut-down layout opens as a table");
        return;
    }
    pv::Binding b;
    check(!pv::resolve_binding(table, "lame", b),
          "a preset without a jump action is refused, not reported as bound");
}

void check_intent(const fi::PresetTable& table) {
    pv::Binding b;
    if (!pv::resolve_binding(table, "default", b)) return;

    const fix32 right = fix32::from_int(1);
    const fix32 up = fix32::from_int(1);
    const fix32 down = fix32::from_int(-1);
    const fix32 tilt = fix32::from_float(-0.3);

    check(pv::read_input(frame_of(b, right, fix32{}, false), b).move_x == right,
          "the move axis reaches the controller as it is");
    check(pv::read_input(frame_of(b, fix32{}, fix32{}, true), b).jump_held,
          "the jump action reaches the controller");

    // Знак: ось ВВЕРХ положительна, а просьба пройти сквозь площадку — вниз.
    check(pv::read_input(frame_of(b, fix32{}, down, false), b).down_held,
          "a stick pulled down asks to drop through");
    check(!pv::read_input(frame_of(b, fix32{}, up, false), b).down_held,
          "a stick pushed up does NOT ask to drop through");

    // Порог: диагональ на стике даёт по вертикали заметно меньше хода, чем намерение спускаться.
    check(!pv::read_input(frame_of(b, right, tilt, false), b).down_held,
          "a diagonal run does not read as a request to drop through");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string path = DEFAULT_MANIFEST;
    for (int i = 1; i < argc; ++i) path = argv[i];

    std::printf("platformer input: the shipped layout, read as the controller reads it\n");

    std::vector<uint8_t> blob;
    fi::PresetBakeError err;
    if (!fi::bake_presets_file(path, blob, err)) {
        std::printf("  FAIL: %s:%d: %s\n", path.c_str(), err.line, err.message.c_str());
        std::printf("game-platformer-input: FAIL\n");
        return 1;
    }
    fi::PresetTable table;
    check(table.open(blob.data(), blob.size()), "the baked layout opens as a table");
    if (table.valid()) {
        check_binding(table);
        check_incomplete();
        check_intent(table);
    }

    std::printf("game-platformer-input: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
