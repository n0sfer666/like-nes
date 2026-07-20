#include <cstdio>
#include <vector>
#include "action_map.hpp"
#include "codes.hpp"
#include "engine.hpp"
#include "sim.hpp"

using namespace input;
namespace c = input::code;

// Логический ввод тика (что «на самом деле» происходит), независимо от частоты опроса.
struct TickInput {
    bool jump;      // Space
    bool fire;      // PadB
    int move_dir;   // -1 A / +1 D / 0
    int mouse_dx;   // суммарная дельта мыши за тик (px)
    int pad_ly_raw; // fix32 raw для оси LY
};

static TickInput scenario(uint32_t t) {
    TickInput in;
    in.jump = (t % 11) < 3;
    in.fire = (t % 13) < 2;
    in.move_dir = static_cast<int>((t / 8) % 3) - 1;
    in.mouse_dx = static_cast<int>((t * 3) % 7) - 3;
    fix32 ly = fix32::from_int(static_cast<int>(t % 20) - 10) / fix32::from_int(10);
    in.pad_ly_raw = ly.raw;
    return in;
}

// Эмиссия сырых событий за тик при заданной плотности опроса (density>=1). Больше density —
// больше избыточных опросов того же уровня / мельче дробление дельты; результат тика ДОЛЖЕН
// совпасть (идемпотентность уровня + сумма дельт).
static uint64_t g_seq = 0;
static void post_btn(InputEngine& e, RawKind down, RawKind up, DeviceKind dev, uint8_t slot, uint16_t code, bool cur, bool prev, int density) {
    if (cur && !prev) e.post({down, dev, slot, code, 0, g_seq++});
    else if (!cur && prev) e.post({up, dev, slot, code, 0, g_seq++});
    if (cur) for (int i = 1; i < density; ++i) e.post({down, dev, slot, code, 0, g_seq++}); // избыточные опросы
}

static void emit_tick(InputEngine& e, const TickInput& cur, const TickInput& prev, int density) {
    post_btn(e, RawKind::KeyDown, RawKind::KeyUp, DeviceKind::Keyboard, 0, c::Space, cur.jump, prev.jump, density);
    post_btn(e, RawKind::PadButtonDown, RawKind::PadButtonUp, DeviceKind::Gamepad, 0, c::PadB, cur.fire, prev.fire, density);
    bool curA = cur.move_dir < 0, prevA = prev.move_dir < 0;
    bool curD = cur.move_dir > 0, prevD = prev.move_dir > 0;
    post_btn(e, RawKind::KeyDown, RawKind::KeyUp, DeviceKind::Keyboard, 0, c::A, curA, prevA, density);
    post_btn(e, RawKind::KeyDown, RawKind::KeyUp, DeviceKind::Keyboard, 0, c::D, curD, prevD, density);
    // Мышь: сумма дельт == mouse_dx независимо от дробления (знак-корректно: base=trunc,
    // |rem| добавок по sign(d) → sum = k*base + rem = d).
    int d = cur.mouse_dx, k = density;
    int base = d / k, rem = d - base * k;                 // |rem| < k, знак = знак d
    for (int i = 0; i < k; ++i) { int part = base + (i < (rem < 0 ? -rem : rem) ? (d >= 0 ? 1 : -1) : 0); e.post({RawKind::MouseMove, DeviceKind::Mouse, 0, c::MAxX, part, g_seq++}); }
    // Ось LY: избыточные обновления, последнее == cur.
    for (int i = 0; i < density; ++i) e.post({RawKind::PadAxis, DeviceKind::Gamepad, 0, c::LY, cur.pad_ly_raw, g_seq++});
}

static ActionMap make_map() {
    ActionMap m;
    m.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    m.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    m.bind(A_Fire, {SourceKind::MouseButton, c::MLeft, 1});
    m.bind(A_Fire, {SourceKind::PadButton, c::PadB, 1});
    m.bind_axis(AX_MoveX, {SourceKind::Key, c::D, 1}, {SourceKind::Key, c::A, 1}, fix32{}, 0);
    // Мышь со scale=1/16 → дельта (0..3 px) остаётся суб-единичной и НЕ клампится dead-zone →
    // ВЕЛИЧИНА суммы дельт входит в sim-hash: гейт реально ловит регресс суммирования/частоты.
    m.bind_axis(AX_AimX, {SourceKind::MouseAxis, c::MAxX, 1}, {SourceKind::None, 0, 0}, fix32{}, 0, fix32::from_float(1.0 / 16));
    m.bind_axis(AX_MoveY, {SourceKind::PadAxis, c::LY, 1}, {SourceKind::None, 0, 0}, fix32::from_float(0.2), 0);
    PlayerAssign pa; pa.use_kbd_mouse = true; pa.pad_slot = 0;
    m.assign_player(0, pa);
    return m;
}

// Прогон сценария: возвращает sim-hash и (опц.) записанный InputFrame-поток.
static uint64_t run(const ActionMap& map, uint32_t ticks, int density, std::vector<InputFrame>* rec) {
    InputEngine e(map);
    if (rec) e.set_recording(true);
    e.device().pad_connected[0] = true;
    SimState st;
    uint64_t h = asset::FNV_OFFSET;
    TickInput prev{false, false, 0, 0, 0};
    g_seq = 0;
    for (uint32_t t = 0; t < ticks; ++t) {
        TickInput cur = scenario(t);
        emit_tick(e, cur, prev, density);
        const InputFrame& f = e.begin_tick(t, 0);
        sim_step(st, f);
        h = hash_state(st, f, h);
        prev = cur;
    }
    if (rec) *rec = e.record();
    return h;
}

static uint64_t replay(const ActionMap& map, const std::vector<InputFrame>& rec) {
    InputEngine e(map);
    SimState st;
    uint64_t h = asset::FNV_OFFSET;
    for (const InputFrame& r : rec) { const InputFrame& f = e.replay_tick(r); sim_step(st, f); h = hash_state(st, f, h); }
    return h;
}

int main() {
    const uint32_t T = 600;
    ActionMap map = make_map();

    std::vector<InputFrame> rec;
    uint64_t h_d1 = run(map, T, 1, &rec);
    uint64_t h_d1b = run(map, T, 1, nullptr);
    uint64_t h_d8 = run(map, T, 8, nullptr);
    uint64_t h_d3 = run(map, T, 3, nullptr);
    uint64_t h_replay = replay(map, rec);

    printf("input-determinism gate:\n");
    printf("  sim_hash (density=1): 0x%016llx\n", (unsigned long long)h_d1);
    printf("  run-to-run: %s\n", h_d1 == h_d1b ? "YES" : "NO");
    printf("  poll-rate independent (d1==d3==d8): %s\n", (h_d1 == h_d3 && h_d1 == h_d8) ? "YES" : "NO");
    printf("  record->replay identical: %s\n", h_d1 == h_replay ? "YES" : "NO");
    printf("  recorded frames: %zu\n", rec.size());

    bool ok = (h_d1 == h_d1b) && (h_d1 == h_d3) && (h_d1 == h_d8) && (h_d1 == h_replay);
    printf("%s\n", ok ? "input-determinism: PASS" : "input-determinism: FAIL");
    return ok ? 0 : 1;
}
