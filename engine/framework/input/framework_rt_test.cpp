#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

#include "codes.hpp"
#include "pad_registry.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"
#include "schedule.hpp"
#include "stick.hpp"

// Гейт 7 спеки #14: тик слоя фреймворка не ходит в кучу. Аллокации живут на старте — бейк,
// открытие таблицы, построение расписания; всё, что повторяется каждый кадр, обязано укладываться
// в уже выделенное. Проверяется тем же приёмом, что горячий путь ввода (#4): глобальный
// operator new считает вызовы внутри отмеченного региона.
namespace {
bool g_in_hot = false;
long g_allocs = 0;
} // namespace

void* operator new(std::size_t n) {
    if (g_in_hot) ++g_allocs;
    void* p = std::malloc(n ? n : 1);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

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
    g_in_hot = true;
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
    g_in_hot = false;

    const bool pass = (g_allocs == 0) && g_ticked == 5000 && sink.raw != 0x7fffffff;
    if (g_allocs != 0)
        std::printf("  FAIL: %ld heap allocations in the framework tick\n", g_allocs);
    std::printf("framework-rt: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
