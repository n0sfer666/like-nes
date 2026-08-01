#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "action_map.hpp"
#include "codes.hpp"
#include "engine.hpp"
#include "sim.hpp"
#include "platform_noinline.hpp"

// Гейт #3 (спека #4): горячий путь дренажа+коалесценции @tick — БЕЗ heap-аллокаций (пулы/
// фикс. очередь преаллоцированы). Плюс устойчивость: hot-unplug / потеря фокуса → force-release
// (нет залипшей кнопки); unmapped/OOB устройство → fallback, не crash. Гоняется под ASan/UBSan.

namespace { bool g_in_hot = false; long g_allocs = 0; }
// Запрет инлайна обязателен, а не косметика — почему, см. platform_noinline.hpp.
PLATFORM_NOINLINE void* operator new(std::size_t n) { if (g_in_hot) ++g_allocs; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
PLATFORM_NOINLINE void* operator new[](std::size_t n) { return operator new(n); }
PLATFORM_NOINLINE void operator delete(void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete[](void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete(void* p, std::size_t) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace input;
namespace c = input::code;

static ActionMap make_map() {
    ActionMap m;
    m.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    m.bind(A_Fire, {SourceKind::PadButton, c::PadB, 1});
    m.bind_axis(AX_MoveX, {SourceKind::Key, c::D, 1}, {SourceKind::Key, c::A, 1}, fix32{}, 0);
    PlayerAssign pa; pa.use_kbd_mouse = true; pa.pad_slot = 0; m.assign_player(0, pa);
    return m;
}

int main() {
    if (!std::atomic<std::size_t>{}.is_lock_free()) { fprintf(stderr, "[input_rt] FAIL: SPSC atomics not lock-free\n"); return 1; }
    ActionMap m = make_map();
    InputEngine e(m); e.device().pad_connected[0] = true;
    e.begin_tick(0); // прогрев буфера (вне горячего региона)

    // --- Горячий регион: post + begin_tick не аллоцируют ---
    g_in_hot = true;
    for (uint32_t t = 1; t <= 5000; ++t) {
        e.post({RawKind::KeyDown, DeviceKind::Keyboard, 0, c::Space, 0, t});
        e.post({RawKind::MouseMove, DeviceKind::Mouse, 0, c::MAxX, static_cast<int32_t>(t % 5) - 2, t});
        e.post({RawKind::PadButtonDown, DeviceKind::Gamepad, 0, c::PadB, 0, t});
        e.post({RawKind::PadAxis, DeviceKind::Gamepad, 0, c::LY, static_cast<int32_t>(t << 4), t});
        e.begin_tick(t);
    }
    g_in_hot = false;
    if (g_allocs != 0) { fprintf(stderr, "[input_rt] FAIL: %ld heap allocations in hot drain/coalesce\n", g_allocs); return 1; }

    // --- Устойчивость 1: hot-unplug активного pad → Fire released, не залипает ---
    InputEngine e2(m); e2.device().pad_connected[0] = true;
    e2.post({RawKind::PadButtonDown, DeviceKind::Gamepad, 0, c::PadB, 0, 1});
    InputFrame f1 = e2.begin_tick(1);
    e2.post({RawKind::DeviceDisconnected, DeviceKind::Gamepad, 0, 0, 0, 2});
    InputFrame f2 = e2.begin_tick(2);
    bool unplug_ok = f1.action_held(A_Fire) && f1.action_pressed(A_Fire) &&
                     !f2.action_held(A_Fire) && f2.action_released(A_Fire);

    // --- Устойчивость 2: потеря фокуса → все held kbd released ---
    InputEngine e3(m);
    e3.post({RawKind::KeyDown, DeviceKind::Keyboard, 0, c::Space, 0, 1});
    InputFrame g1 = e3.begin_tick(1);
    e3.post({RawKind::FocusLost, DeviceKind::None, 0, 0, 0, 2});
    InputFrame g2 = e3.begin_tick(2);
    bool focus_ok = g1.action_held(A_Jump) && !g2.action_held(A_Jump) && g2.action_released(A_Jump);

    // --- Устойчивость 3: unmapped-код + OOB slot/axis → без эффекта, без crash (ASan verifies) ---
    InputEngine e4(m); e4.device().pad_connected[0] = true;
    e4.post({RawKind::PadButtonDown, DeviceKind::Gamepad, 0, 20, 0, 1});   // код без биндинга
    e4.post({RawKind::PadButtonDown, DeviceKind::Gamepad, 40, c::PadB, 0, 2}); // OOB slot
    e4.post({RawKind::PadAxis, DeviceKind::Gamepad, 0, 99, 123, 3});        // OOB axis code
    e4.post({RawKind::KeyDown, DeviceKind::Keyboard, 0, 9999, 0, 4});       // OOB key
    const InputFrame& q = e4.begin_tick(1);
    bool fallback_ok = (q.held == 0); // ничего не сматчилось, живы

    printf("input-rt gate:\n");
    printf("  no-alloc hot drain/coalesce (5000 ticks): YES (allocs=%ld)\n", g_allocs);
    printf("  hot-unplug force-release: %s\n", unplug_ok ? "YES" : "NO");
    printf("  focus-loss force-release: %s\n", focus_ok ? "YES" : "NO");
    printf("  unmapped/OOB fallback (no crash): %s\n", fallback_ok ? "YES" : "NO");
    bool ok = unplug_ok && focus_ok && fallback_ok;
    printf("%s\n", ok ? "input-rt: PASS" : "input-rt: FAIL");
    return ok ? 0 : 1;
}
