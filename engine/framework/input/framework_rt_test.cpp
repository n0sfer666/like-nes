#include <cstdio>
#include <vector>

#include "codes.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "pad_registry.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"
#include "schedule.hpp"
#include "stick.hpp"

// Гейт 7 спеки #14: тик слоя фреймворка не ходит в кучу. Аллокации живут на старте — бейк,
// открытие таблицы, построение расписания; всё, что повторяется каждый кадр, обязано укладываться
// в уже выделенное. Счётчик — общий с гейтом 6 спеки #15, одной реализацией.
namespace {

const char* MANIFEST = R"(
preset | default
action | fire | key:space | pad:south
axis   | move_x | key:d | key:a
axis   | move_x | padaxis:lx | -
axis   | move_y | key:w | key:s
axis   | move_y | padaxis:-ly | -
shape  | move_x | 0.18 | 1.0 | 2 | move_y
shape  | move_y | 0.18 | 1.0 | 2 | move_x

pad | Microsoft Xbox | 0x045e | - | Xbox | xbox | 0.18 | 0.12
)";

int g_ticked = 0;

void tick_system(void*, const framework::Tick&) { ++g_ticked; }

} // namespace

int main() {
    using namespace framework::input;
    namespace c = ::input::code;

    std::vector<uint8_t> blob;
    PresetBakeError err;
    if (!bake_presets(MANIFEST, blob, err)) {
        std::printf("  FAIL: bake failed at line %d: %s\n", err.line, err.message.c_str());
        return 1;
    }
    PresetTable table;
    if (!table.open(blob.data(), blob.size())) {
        std::printf("  FAIL: table did not open\n");
        return 1;
    }
    ::input::ActionMap map;
    if (!table.bind(0, map)) {
        std::printf("  FAIL: preset did not bind\n");
        return 1;
    }
    ::input::PlayerAssign assign;
    assign.use_kbd_mouse = true;
    assign.pad_slot = 0;
    map.assign_player(0, assign);

    framework::Schedule sched;
    framework::SystemDesc d;
    d.name = "input";
    d.stage = framework::Stage::Sim;
    d.fn = tick_system;
    if (!sched.add(d) || sched.build() != framework::BuildResult::Ok) {
        std::printf("  FAIL: schedule did not build\n");
        return 1;
    }

    PadRegistry reg;
    ::input::PadInfo info;
    info.vid = 0x045e;
    const PadProfile& profile = (reg.connected(0, info, table), reg.profile(0));

    ::input::DeviceState state;
    state.pad_connected[0] = true;
    const StickShape shape = table.axis_shape(0, 0);
    fix32 sink;
    uint64_t prev = 0;

    // --- Горячий регион: разрешение действий, форма стика, профиль слота ---
    framework::probe::in_hot = true;
    for (uint32_t t = 1; t <= 5000; ++t) {
        state.pad_axes[0][c::LX] = fix32::from_raw(static_cast<int32_t>(t) * 13);
        state.pad_axes[0][c::LY] = fix32::from_raw(static_cast<int32_t>(t) * 7);
        const ::input::InputFrame f = map.resolve(state, 0, t, prev);
        prev = f.held;
        const Vec2 v = radial({f.axes[0], f.axes[1]}, shape);
        sink = sink + v.x + v.y +
               trigger(fix32::from_raw(static_cast<int32_t>(t)), profile.trigger_threshold);
        framework::Tick tick;
        tick.index = t;
        sched.run(tick);
        reg.profile(0);
    }
    framework::probe::in_hot = false;
    const long in_tick = framework::probe::allocs;

    // Позитивный контроль счётчика. Без него ноль выше неотличим от неработающего перехвата, а
    // неработающий перехват выглядит как самый зелёный гейт на свете — и выглядел им здесь на всех
    // трёх ОС с самого появления гейта. Замещение операторов действует на ПРОГРАММУ, поэтому
    // доказательство из физической цели сюда не переносится: у этого бинаря оно своё.
    //
    // Выделение зовётся через непрозрачную границу — тело в `framework_alloc_probe_control.cpp`.
    // Строкой `new` по месту контроль вакуумен ровно там, где нужен: и clang, и gcc выбрасывают
    // пару new/delete, результат которой не наблюдаем, разбор и замеры — в шапке заголовка.
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool control_ok = framework::probe::control::plain_allocation();
    const long control = framework::probe::allocs;
    framework::probe::in_hot = false;

    const bool pass = in_tick == 0 && control > 0 && control_ok && g_ticked == 5000 &&
                      sink.raw != 0x7fffffff;
    if (in_tick != 0)
        std::printf("  FAIL: %ld heap allocations in the framework tick\n", in_tick);
    if (control <= 0 || !control_ok)
        std::printf("  FAIL: control allocation not seen (allocs=%ld, handed out=%d)\n", control,
                    control_ok ? 1 : 0);
    std::printf("framework-rt: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
