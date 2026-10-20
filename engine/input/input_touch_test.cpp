#include <cstdio>
#include "action_map.hpp"
#include "codes.hpp"
#include "input_sim.hpp"

// Экранный тач в ядре ввода (спека #26 В2а): кнопки и ось стика — источники карты, как у пада, но
// устройство одно на экран, поэтому два игрока на таче — отказ, а не тихое удвоение.
using namespace input;
namespace c = input::code;

static const Source TOUCH_A{SourceKind::TouchButton, c::TouchA, 1};
static const Source STICK_X{SourceKind::TouchAxis, c::StickX, 1};

static RawEvent ev(RawKind k, uint16_t code, int32_t value = 0) { return {k, DeviceKind::Touch, 0, code, value, 0}; }

static ActionLayout touch_layout() {
    ActionLayout l;
    l.bind(A_Jump, TOUCH_A);
    l.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    l.bind_axis(AX_MoveX, STICK_X, {}, fix32{}, 0);
    l.bind_axis(AX_MoveY, {SourceKind::TouchAxis, c::StickY, -1}, {}, fix32{}, 0);
    return l;
}

static bool test_enum_numbers_kept() {
    return static_cast<int>(SourceKind::PadAxis) == 5 && static_cast<int>(SourceKind::TouchButton) == 6 &&
           static_cast<int>(SourceKind::TouchAxis) == 7 && static_cast<int>(RawKind::TickMark) == 12 &&
           static_cast<int>(DeviceKind::Gamepad) == 3;
}

static bool test_touch_reaches_its_player_only() {
    ActionMap m;
    if (!m.set_layout(0, touch_layout()) || !m.set_layout(1, touch_layout())) return false;
    if (!m.assign_player(0, PlayerAssign{.use_touch = true})) return false;
    if (!m.assign_player(1, PlayerAssign{.use_kbd_mouse = true})) return false;
    DeviceState d;
    d.apply(ev(RawKind::TouchButtonDown, c::TouchA));
    return m.resolve(d, 0, 1, 0).action_held(A_Jump) && !m.resolve(d, 1, 1, 0).action_held(A_Jump);
}

static bool test_stick_axis_keeps_sign() {
    ActionMap m;
    if (!m.set_layout(0, touch_layout()) || !m.assign_player(0, PlayerAssign{.use_touch = true})) return false;
    DeviceState d;
    const fix32 half = fix32::from_raw(fix32::ONE / 2);
    d.apply(ev(RawKind::TouchAxis, c::StickX, half.raw));
    d.apply(ev(RawKind::TouchAxis, c::StickY, half.raw));
    const InputFrame f = m.resolve(d, 0, 1, 0);
    return f.axes[AX_MoveX] == half && f.axes[AX_MoveY] == -half && m.resolve(d, 1, 1, 0).axes[AX_MoveX] == fix32{};
}

static bool test_one_screen_one_player() {
    ActionMap m;
    if (!m.assign_player(0, PlayerAssign{.use_touch = true})) return false;
    SharedInput shared;
    const bool refused = !m.assign_player(1, PlayerAssign{.use_touch = true}, &shared);
    const bool named = shared.player == 1 && shared.other == 0 && shared.src.kind == SourceKind::None && shared.screen;
    return refused && named && !m.assignment(1).use_touch && m.assign_player(1, PlayerAssign{.pad_slot = 0});
}

static bool test_pad_and_touch_together() {
    ActionMap m;
    if (!m.set_layout(0, touch_layout()) || !m.assign_player(0, PlayerAssign{.pad_slot = 2, .use_touch = true}))
        return false;
    DeviceState pad;
    pad.apply({RawKind::PadButtonDown, DeviceKind::Gamepad, 2, static_cast<uint16_t>(c::PadA), 0, 0});
    DeviceState touch;
    touch.apply(ev(RawKind::TouchButtonDown, c::TouchA));
    return m.resolve(pad, 0, 1, 0).action_held(A_Jump) && m.resolve(touch, 0, 1, 0).action_held(A_Jump);
}

static bool test_release_and_focus_clear_touch() {
    DeviceState d;
    d.apply(ev(RawKind::TouchButtonDown, c::TouchE));
    d.apply(ev(RawKind::TouchButtonDown, static_cast<uint16_t>(TOUCH_BTNS)));
    d.apply(ev(RawKind::TouchAxis, static_cast<uint16_t>(TOUCH_AXES), fix32::ONE));
    const bool bounded = d.touch_down(c::TouchE) && d.touch_btns == (1u << c::TouchE) && !d.touch_down(TOUCH_BTNS);
    d.apply(ev(RawKind::TouchButtonUp, c::TouchE));
    const bool released = !d.touch_down(c::TouchE);
    d.apply(ev(RawKind::TouchButtonDown, c::TouchB));
    d.apply(ev(RawKind::TouchAxis, c::StickY, fix32::ONE));
    d.apply({RawKind::FocusLost, DeviceKind::None, 0, 0, 0, 0});
    return bounded && released && d.touch_btns == 0 && d.touch_axis(c::StickY) == fix32{};
}

static bool test_capture_touch_button() {
    const Source s = capture_source(ev(RawKind::TouchButtonDown, c::TouchC));
    return s.kind == SourceKind::TouchButton && s.code == c::TouchC && s.sign == 1 &&
           capture_source(ev(RawKind::TouchAxis, c::StickX, fix32::ONE)).kind == SourceKind::None;
}

int main() {
    struct { const char* n; bool ok; } t[] = {
        {"enum numbers kept", test_enum_numbers_kept()},
        {"touch reaches its player", test_touch_reaches_its_player_only()},
        {"stick axis keeps sign", test_stick_axis_keeps_sign()},
        {"one screen, one player", test_one_screen_one_player()},
        {"pad and touch together", test_pad_and_touch_together()},
        {"release and focus clear", test_release_and_focus_clear_touch()},
        {"capture a touch button", test_capture_touch_button()},
    };
    printf("input-touch gate:\n");
    bool all = true;
    for (auto& x : t) { printf("  %-28s %s\n", x.n, x.ok ? "YES" : "NO"); all &= x.ok; }
    printf("%s\n", all ? "input-touch: PASS - 7 cases" : "input-touch: FAIL");
    return all ? 0 : 1;
}
