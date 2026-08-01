#include <atomic>
#include <cstdio>
#include <thread>
#include "action_map.hpp"
#include "codes.hpp"
#include "engine.hpp"
#include "sim.hpp"

using namespace input;
namespace c = input::code;

struct TickInput { bool jump, fire; int move_dir, mouse_dx, pad_ly_raw; };

static TickInput scenario(uint32_t t) {
    TickInput in;
    in.jump = (t % 11) < 3; in.fire = (t % 13) < 2;
    in.move_dir = static_cast<int>((t / 8) % 3) - 1;
    in.mouse_dx = static_cast<int>((t * 3) % 7) - 3;
    in.pad_ly_raw = (fix32::from_int(static_cast<int>(t % 20) - 10) / fix32::from_int(10)).raw;
    return in;
}

static uint64_t g_seq = 0;
static void btn(InputEngine& e, RawKind dn, RawKind up, DeviceKind dv, uint8_t sl, uint16_t cd, bool cur, bool prev) {
    if (cur && !prev) e.post({dn, dv, sl, cd, 0, g_seq++});
    else if (!cur && prev) e.post({up, dv, sl, cd, 0, g_seq++});
}
static void emit(InputEngine& e, const TickInput& cur, const TickInput& prev) {
    btn(e, RawKind::KeyDown, RawKind::KeyUp, DeviceKind::Keyboard, 0, c::Space, cur.jump, prev.jump);
    btn(e, RawKind::PadButtonDown, RawKind::PadButtonUp, DeviceKind::Gamepad, 0, c::PadB, cur.fire, prev.fire);
    btn(e, RawKind::KeyDown, RawKind::KeyUp, DeviceKind::Keyboard, 0, c::A, cur.move_dir < 0, prev.move_dir < 0);
    btn(e, RawKind::KeyDown, RawKind::KeyUp, DeviceKind::Keyboard, 0, c::D, cur.move_dir > 0, prev.move_dir > 0);
    e.post({RawKind::MouseMove, DeviceKind::Mouse, 0, c::MAxX, cur.mouse_dx, g_seq++});
    e.post({RawKind::PadAxis, DeviceKind::Gamepad, 0, c::LY, cur.pad_ly_raw, g_seq++});
    e.post({RawKind::TickMark, DeviceKind::None, 0, 0, 0, g_seq++});
}

static ActionMap make_map() {
    ActionMap m;
    m.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    m.bind(A_Fire, {SourceKind::PadButton, c::PadB, 1});
    m.bind_axis(AX_MoveX, {SourceKind::Key, c::D, 1}, {SourceKind::Key, c::A, 1}, fix32{}, 0);
    m.bind_axis(AX_AimX, {SourceKind::MouseAxis, c::MAxX, 1}, {SourceKind::None, 0, 0}, fix32{}, 0);
    m.bind_axis(AX_MoveY, {SourceKind::PadAxis, c::LY, 1}, {SourceKind::None, 0, 0}, fix32::from_float(0.2), 0);
    PlayerAssign pa; pa.use_kbd_mouse = true; pa.pad_slot = 0; m.assign_player(0, pa);
    return m;
}

static uint64_t single_thread(uint32_t T) {
    ActionMap m = make_map();
    InputEngine e(m); e.device().pad_connected[0] = true;
    SimState st; uint64_t h = asset::FNV_OFFSET; TickInput prev{false, false, 0, 0, 0}; g_seq = 0;
    for (uint32_t t = 0; t < T; ++t) {
        TickInput cur = scenario(t); emit(e, cur, prev);
        while (!e.begin_tick_marked(t, 0)) {}
        sim_step(st, e.frame()); h = hash_state(st, e.frame(), h); prev = cur;
    }
    return h;
}

// `(void)x` — не украшение: GCC 16 распространил -Wunused-but-set-variable на volatile, а под
// -Werror это валит сборку. Читать x обязано что-то, иначе диагностика права; сам volatile нужен,
// чтобы цикл дожил до рантайма и задержка была настоящей.
static void spin(uint32_t n) { volatile uint32_t x = 0; for (uint32_t i = 0; i < n; ++i) x += i; (void)x; }

static uint64_t threaded(uint32_t T) {
    ActionMap m = make_map();
    InputEngine e(m); e.device().pad_connected[0] = true;
    std::atomic<uint32_t> consumer_tick{0};
    constexpr uint32_t AHEAD = 8; // продюсер не убегает дальше → очередь ограничена
    g_seq = 0;

    std::thread producer([&] {
        TickInput prev{false, false, 0, 0, 0};
        for (uint32_t t = 0; t < T; ++t) {
            while (t >= consumer_tick.load(std::memory_order_acquire) + AHEAD) spin(64);
            spin((t % 5) * 40); // джиттер продюсера
            emit(e, scenario(t), prev); prev = scenario(t);
        }
    });

    SimState st; uint64_t h = asset::FNV_OFFSET;
    for (uint32_t t = 0; t < T; ++t) {
        spin((t % 3) * 30); // джиттер консюмера
        while (!e.begin_tick_marked(t, 0)) spin(64);
        sim_step(st, e.frame()); h = hash_state(st, e.frame(), h);
        consumer_tick.store(t + 1, std::memory_order_release);
    }
    producer.join();
    return h;
}

int main() {
    const uint32_t T = 600;
    uint64_t h_ref = single_thread(T);
    uint64_t h_thr = threaded(T);
    printf("input-race gate:\n");
    printf("  sim_hash single-thread: 0x%016llx\n", (unsigned long long)h_ref);
    printf("  sim_hash threaded(jitter): 0x%016llx\n", (unsigned long long)h_thr);
    printf("  timing-independent (one-way SPSC): %s\n", h_ref == h_thr ? "YES" : "NO");
    bool ok = h_ref == h_thr;
    printf("%s\n", ok ? "input-race: PASS" : "input-race: FAIL");
    return ok ? 0 : 1;
}
