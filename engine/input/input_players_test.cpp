#include <cstdio>
#include "action_map.hpp"
#include "codes.hpp"
#include "input_engine.hpp"
#include "input_sim.hpp"

// Ввод на игрока (спека #25 В4а): дренаж раз за тик, resolve на каждого игрока со своим prev_held.
// «Игрок 0 как раньше» держат голдены #4 и #14, здесь — что игрок 1 его не задевает. Свойства
// прогоняются шаблоном и на InputEngine, и на фикстуре с общим prev_held — той самой ошибке, от
// которой гейт защищает: свойство, которое фикстура проходит, ничего не проверяет.
using namespace input;
namespace c = input::code;

constexpr uint32_t TICKS = 6;
constexpr uint32_t P1_PRESS_TICK = 3;

static RawEvent ev(RawKind k, DeviceKind d, uint16_t code, int32_t value = 0) { return {k, d, 0, code, value, 0}; }

static const fix32 MOUSE_SCALE = fix32::from_float(1.0 / 16);

static bool bind_hotseat(ActionMap& m) {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    l.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    l.bind_axis(AX_AimX, {SourceKind::MouseAxis, c::MAxX, 1}, {}, fix32{}, 0, MOUSE_SCALE);
    ActionLayout mouse_y;
    mouse_y.bind_axis(AX_AimX, {SourceKind::MouseAxis, c::MAxY, 1}, {}, fix32{}, 0, MOUSE_SCALE);
    PlayerAssign kbd; kbd.use_kbd_mouse = true;
    PlayerAssign pad; pad.pad_slot = 0;
    return m.set_layout(0, l) && m.set_layout(1, l) && m.set_layout(2, mouse_y) && m.assign_player(0, kbd) &&
           m.assign_player(1, pad) && m.assign_player(2, kbd);
}

class SharedPrevHeld {
public:
    explicit SharedPrevHeld(const ActionMap& m) : map_(m) {}
    void post(const RawEvent& e) { device_.apply(e); }
    void drain() { device_.latch_frame_delta(); }
    InputFrame resolve(uint32_t tick, int player) {
        InputFrame f = map_.resolve(device_, player, tick, prev_held_);
        prev_held_ = f.held;
        return f;
    }

private:
    const ActionMap& map_;
    DeviceState device_;
    uint64_t prev_held_ = 0;
};

static bool same(const InputFrame& a, const InputFrame& b) {
    if (a.tick != b.tick || a.held != b.held || a.pressed != b.pressed || a.released != b.released) return false;
    for (int i = 0; i < MAX_AXES; ++i)
        if (!(a.axes[i] == b.axes[i])) return false;
    return true;
}

template <class E>
static void post_script(uint32_t t, E& e) {
    if (t == 0) {
        e.post(ev(RawKind::DeviceConnected, DeviceKind::Gamepad, 0));
        e.post(ev(RawKind::KeyDown, DeviceKind::Keyboard, c::Space));
    }
    if (t == P1_PRESS_TICK) e.post(ev(RawKind::PadButtonDown, DeviceKind::Gamepad, c::PadA));
}

static bool edges_match_script(uint32_t t, const InputFrame& p0, const InputFrame& p1) {
    bool p0_ok = p0.action_held(A_Jump) && p0.action_pressed(A_Jump) == (t == 0) && !p0.action_released(A_Jump);
    bool p1_ok = p1.action_held(A_Jump) == (t >= P1_PRESS_TICK) && p1.action_pressed(A_Jump) == (t == P1_PRESS_TICK) &&
                 !p1.action_released(A_Jump);
    return p0_ok && p1_ok;
}

template <class E>
static bool pressed_independent(E& e) {
    bool ok = true;
    for (uint32_t t = 0; t < TICKS; ++t) {
        post_script(t, e);
        e.drain();
        InputFrame p0 = e.resolve(t, 0);
        InputFrame p1 = e.resolve(t, 1);
        ok &= edges_match_script(t, p0, p1);
    }
    return ok;
}

template <class E>
static bool order_free(E& forward, E& backward) {
    bool ok = true;
    for (uint32_t t = 0; t < TICKS; ++t) {
        post_script(t, forward);
        post_script(t, backward);
        forward.drain();
        backward.drain();
        InputFrame f0 = forward.resolve(t, 0), f1 = forward.resolve(t, 1);
        InputFrame b1 = backward.resolve(t, 1), b0 = backward.resolve(t, 0);
        ok &= same(f0, b0) && same(f1, b1);
    }
    return ok;
}

static bool test_pressed_independent(const ActionMap& m) { InputEngine e(m); return pressed_independent(e); }

static bool test_order_free(const ActionMap& m) { InputEngine a(m), b(m); return order_free(a, b); }

static bool test_player0_unaffected(const ActionMap& m) {
    InputEngine solo(m), hotseat(m);
    solo.set_recording(true);
    hotseat.set_recording(true);
    bool ok = true;
    for (uint32_t t = 0; t < TICKS; ++t) {
        post_script(t, solo);
        post_script(t, hotseat);
        solo.post(ev(RawKind::MouseMove, DeviceKind::Mouse, c::MAxX, static_cast<int32_t>(t) + 1));
        hotseat.post(ev(RawKind::MouseMove, DeviceKind::Mouse, c::MAxX, static_cast<int32_t>(t) + 1));
        InputFrame want = solo.begin_tick(t);
        hotseat.drain();
        hotseat.resolve(t, 1);
        InputFrame got = hotseat.resolve(t, 0);
        ok &= same(want, got);
    }
    const auto& a = solo.record(0);
    const auto& b = hotseat.record(0);
    ok &= a.size() == TICKS && b.size() == TICKS;
    for (size_t i = 0; ok && i < a.size(); ++i) ok &= same(a[i], b[i]);
    return ok && hotseat.record(1).size() == TICKS && solo.record(1).empty();
}

static bool test_mouse_latched_once(const ActionMap& m) {
    InputEngine e(m);
    e.post(ev(RawKind::MouseMove, DeviceKind::Mouse, c::MAxX, 3));
    e.post(ev(RawKind::MouseMove, DeviceKind::Mouse, c::MAxY, 5));
    e.drain();
    InputFrame p0 = e.resolve(0, 0);
    InputFrame p2 = e.resolve(0, 2);
    InputFrame again = e.resolve(0, 0);
    return p0.axes[AX_AimX] == fix32::from_int(3) * MOUSE_SCALE && p2.axes[AX_AimX] == fix32::from_int(5) * MOUSE_SCALE &&
           same(p0, again);
}

static bool test_slot_untouched(const ActionMap& m) {
    InputEngine e(m);
    e.set_recording(true);
    InputFrame last_p1;
    for (uint32_t t = 0; t < TICKS; ++t) {
        post_script(t, e);
        e.drain();
        last_p1 = e.resolve(t, 1);
    }
    bool p0_clean = e.record(0).empty() && e.buffer(0).count() == 0 && same(e.frame(0), InputFrame{});
    bool p1_read = same(e.frame(1), last_p1) && !same(e.frame(1), e.frame(0));
    InputFrame rec = last_p1;
    rec.tick = TICKS;
    rec.pressed = 1ull << A_Fire;
    e.replay_tick(rec, 1);
    bool p1_replayed = same(e.frame(1), rec) && e.buffer(1).count() == TICKS + 1 && e.record(1).size() == TICKS &&
                       e.record(0).empty() && e.buffer(0).count() == 0 && same(e.frame(0), InputFrame{});
    bool p1_kept = e.record(1).size() == TICKS && e.buffer(1).pressed_within(A_Jump, TICKS) && !e.buffer(0).pressed_within(A_Jump, TICKS);
    InputFrame none_lo = e.resolve(TICKS, -1);
    InputFrame none_hi = e.replay_tick(e.frame(1), MAX_PLAYERS);
    bool outside_dropped = same(none_lo, InputFrame{}) && same(none_hi, InputFrame{}) && e.record(-1).empty() &&
                           e.buffer(MAX_PLAYERS).count() == 0 && e.record(1).size() == TICKS;
    return p0_clean && p1_read && p1_replayed && p1_kept && outside_dropped;
}

static bool test_shared_prev_held_caught(const ActionMap& m) {
    SharedPrevHeld broken(m), forward(m), backward(m);
    return !pressed_independent(broken) && !order_free(forward, backward);
}

int main() {
    ActionMap m;
    if (!bind_hotseat(m)) { printf("input-players: FAIL - hotseat map refused\n"); return 1; }
    struct { const char* n; bool ok; } t[] = {
        {"p2 pressed ignores p1", test_pressed_independent(m)},
        {"resolve order free", test_order_free(m)},
        {"p0 unaffected by p1", test_player0_unaffected(m)},
        {"mouse latched once a tick", test_mouse_latched_once(m)},
        {"other slots untouched", test_slot_untouched(m)},
        {"shared prev_held caught", test_shared_prev_held_caught(m)},
    };
    printf("input-players gate:\n");
    bool all = true;
    for (auto& x : t) { printf("  %-28s %s\n", x.n, x.ok ? "YES" : "NO"); all &= x.ok; }
    printf("%s\n", all ? "input-players: PASS - 6 cases" : "input-players: FAIL");
    return all ? 0 : 1;
}
