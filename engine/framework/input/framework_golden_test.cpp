#include <cstdio>
#include <vector>

#include "codes.hpp"
#include "engine.hpp"
#include "platform_args.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"

// Гейт 1 спеки #14: записанная последовательность сырых событий, разрешённая через пресет из
// бандла, даёт один и тот же хеш кадров. Хеш целочисленный и считается по полям InputFrame,
// поэтому обязан совпасть на трёх ОС — эталон проверяется в CI на каждой из них.
//
// Независимость от частоты опроса даёт коалесценция @tick из #4 (там же и проверена): кадр
// собирается из состояния устройств на границе тика, а не из числа событий внутри него. Здесь
// проверяется то, что добавил слой, — что раскладка приезжает из данных и не шевелит байты.
namespace {

const char* MANIFEST = R"(
preset | default
action | fire | key:space | pad:south
axis   | move_x | key:d | key:a
axis   | move_x | padaxis:lx | -
axis   | move_y | key:w | key:s
shape  | move_x | 0.18 | 1.0 | 1 | -
)";

struct Ev {
    ::input::RawKind kind;
    uint16_t code;
    int32_t value;
};

// Сценарий авторский: удержание и отпускание, перекрытие клавиш по оси, ход стика через мёртвую
// зону и обратно, кнопка пада поверх клавиши того же действия.
const Ev SCRIPT[] = {
    {::input::RawKind::KeyDown, ::input::code::Space, 0},
    {::input::RawKind::KeyDown, ::input::code::D, 0},
    {::input::RawKind::PadAxis, ::input::code::LX, 6000},
    {::input::RawKind::KeyDown, ::input::code::A, 0},
    {::input::RawKind::PadAxis, ::input::code::LX, 40000},
    {::input::RawKind::KeyUp, ::input::code::D, 0},
    {::input::RawKind::PadButtonDown, ::input::code::PadA, 0},
    {::input::RawKind::KeyUp, ::input::code::Space, 0},
    {::input::RawKind::PadAxis, ::input::code::LX, -52000},
    {::input::RawKind::KeyDown, ::input::code::W, 0},
    {::input::RawKind::PadButtonUp, ::input::code::PadA, 0},
    {::input::RawKind::KeyUp, ::input::code::A, 0},
};

uint64_t mix(uint64_t h, uint64_t v) { return (h ^ v) * 0x100000001b3ull; }

uint64_t run(const ::input::ActionMap& map) {
    ::input::InputEngine e(map);
    e.device().pad_connected[0] = true;
    uint64_t h = 0xcbf29ce484222325ull;
    const int n = static_cast<int>(sizeof(SCRIPT) / sizeof(SCRIPT[0]));
    int i = 0;
    for (uint32_t t = 0; i < n || t < 16; ++t) {
        if (i < n) {
            ::input::RawEvent ev;
            ev.kind = SCRIPT[i].kind;
            ev.device = SCRIPT[i].kind == ::input::RawKind::KeyDown ||
                                SCRIPT[i].kind == ::input::RawKind::KeyUp
                            ? ::input::DeviceKind::Keyboard
                            : ::input::DeviceKind::Gamepad;
            ev.code = SCRIPT[i].code;
            ev.value = SCRIPT[i].value;
            ev.seq = t;
            e.post(ev);
            ++i;
        }
        const ::input::InputFrame& f = e.begin_tick(t, 0);
        h = mix(h, f.held);
        h = mix(h, f.pressed);
        h = mix(h, f.released);
        h = mix(h, static_cast<uint64_t>(static_cast<uint32_t>(f.axes[0].raw)));
        h = mix(h, static_cast<uint64_t>(static_cast<uint32_t>(f.axes[1].raw)));
    }
    return h;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;

    std::vector<uint8_t> blob;
    PresetBakeError err;
    if (!bake_presets(MANIFEST, blob, err)) {
        std::printf("  FAIL: bake failed at line %d: %s\n", err.line, err.message.c_str());
        return 1;
    }
    PresetTable table;
    ::input::ActionMap map;
    if (!table.open(blob.data(), blob.size()) || !table.bind(0, map)) {
        std::printf("  FAIL: preset did not load\n");
        return 1;
    }
    ::input::PlayerAssign assign;
    assign.use_kbd_mouse = true;
    assign.pad_slot = 0;
    map.assign_player(0, assign);

    const uint64_t one = run(map);
    const uint64_t two = run(map);
    std::printf("[framework-golden] preset-input-hash = 0x%016llx\n",
                static_cast<unsigned long long>(one));

    bool pass = true;
    if (one != two) {
        std::printf("  FAIL: two runs of the same script disagree\n");
        pass = false;
    }
    std::printf("framework-golden: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
