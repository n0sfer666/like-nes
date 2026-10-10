#include <cstdio>
#include <initializer_list>
#include "action_map.hpp"
#include "codes.hpp"
#include "input_sim.hpp"

// Раскладка на игрока (спека #25 В4б): две раскладки на одной клавиатуре, отказ при общем
// физическом входе и пустой кадр игроку вне диапазона вместо раскладки игрока 0.
using namespace input;
namespace c = input::code;

enum { A_Punch = 2 };
enum : uint16_t { KEY_F = 70, KEY_G = 71, KEY_J = 74, KEY_RIGHT = 262, KEY_LEFT = 263, KP_0 = 320, KP_1 = 321 };

static DeviceState keys_down(std::initializer_list<int> keys) {
    DeviceState d;
    for (int k : keys) d.keys[k >> 6] |= (1ull << (k & 63));
    return d;
}

static DeviceState pad_with(int slot, int btn) {
    DeviceState d;
    d.pad_connected[slot] = true;
    d.pad_btns[slot] |= (1u << btn);
    return d;
}

static ActionLayout left_hand() {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    l.bind(A_Punch, {SourceKind::Key, KEY_J, 1});
    l.bind_axis(AX_MoveX, {SourceKind::Key, c::D, 1}, {SourceKind::Key, c::A, 1}, fix32{});
    return l;
}

static ActionLayout right_hand() {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::Key, KP_0, 1});
    l.bind(A_Punch, {SourceKind::Key, KP_1, 1});
    l.bind_axis(AX_MoveX, {SourceKind::Key, KEY_RIGHT, 1}, {SourceKind::Key, KEY_LEFT, 1}, fix32{});
    return l;
}

static ActionLayout pad_layout() {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    return l;
}

static PlayerAssign keyboard() { return {.pad_slot = -1, .use_kbd_mouse = true}; }
static PlayerAssign pad(int slot) { return {.pad_slot = slot, .use_kbd_mouse = false}; }

static bool one_keyboard_map(ActionMap& m) {
    return m.set_layout(0, left_hand()) && m.set_layout(1, right_hand()) && m.assign_player(0, keyboard()) &&
           m.assign_player(1, keyboard());
}

static bool test_one_keyboard() {
    ActionMap m;
    if (!one_keyboard_map(m)) return false;
    InputFrame j0 = m.resolve(keys_down({KEY_J}), 0, 0, 0), j1 = m.resolve(keys_down({KEY_J}), 1, 0, 0);
    InputFrame k0 = m.resolve(keys_down({KP_1, KEY_LEFT}), 0, 0, 0), k1 = m.resolve(keys_down({KP_1, KEY_LEFT}), 1, 0, 0);
    bool p1_only = j0.action_pressed(A_Punch) && j1.held == 0;
    bool p2_only = k1.action_pressed(A_Punch) && k1.axes[AX_MoveX] == fix32::from_int(-1) && k0.held == 0 &&
                   k0.axes[AX_MoveX] == fix32{};
    return p1_only && p2_only;
}

static bool refused_names(const SharedInput& sh, int player, int other, SourceKind kind, uint16_t code, int slot) {
    return sh.player == player && sh.other == other && sh.src.kind == kind && sh.src.code == code && sh.pad_slot == slot;
}

static bool test_shared_key_refused() {
    ActionMap m;
    if (!one_keyboard_map(m)) return false;
    ActionLayout bad = right_hand();
    bad.bind(A_Punch, {SourceKind::Key, KEY_J, 1});
    SharedInput sh;
    bool refused = !m.set_layout(1, bad, &sh) && refused_names(sh, 1, 0, SourceKind::Key, KEY_J, -1);
    bool kept = m.resolve(keys_down({KP_1}), 1, 0, 0).action_held(A_Punch) && m.resolve(keys_down({KEY_J}), 1, 0, 0).held == 0;
    return refused && kept;
}

static bool test_key_as_axis_counts() {
    ActionMap m;
    if (!one_keyboard_map(m)) return false;
    ActionLayout bad = right_hand();
    bad.bind_axis(AX_MoveY, {SourceKind::Key, c::W, 1}, {SourceKind::Key, c::A, 1}, fix32{});
    SharedInput sh;
    return !m.set_layout(1, bad, &sh) && refused_names(sh, 1, 0, SourceKind::Key, c::A, -1);
}

static bool test_context_does_not_split() {
    ActionMap m;
    ActionLayout menu;
    menu.bind(A_Punch, {SourceKind::Key, KEY_J, 1}, 7);
    SharedInput sh;
    return one_keyboard_map(m) && !m.set_layout(1, menu, &sh) && refused_names(sh, 1, 0, SourceKind::Key, KEY_J, -1);
}

static bool test_assignment_refused() {
    ActionMap m;
    bool set = m.set_layout(0, left_hand()) && m.assign_player(0, keyboard()) && m.set_layout(1, left_hand()) &&
               m.assign_player(1, pad(0));
    SharedInput sh;
    bool refused = !m.assign_player(1, keyboard(), &sh) && sh.player == 1 && sh.other == 0 && sh.src.kind == SourceKind::Key;
    bool kept = m.resolve(keys_down({KEY_J}), 1, 0, 0).held == 0;
    return set && refused && kept;
}

static bool test_pad_slots() {
    ActionMap m;
    bool two_pads = m.set_layout(0, pad_layout()) && m.set_layout(1, pad_layout()) && m.assign_player(0, pad(0)) &&
                    m.assign_player(1, pad(1));
    SharedInput sh;
    bool same_slot = !m.assign_player(1, pad(0), &sh) && refused_names(sh, 1, 0, SourceKind::PadButton, c::PadA, 0);
    bool kept = m.resolve(pad_with(1, c::PadA), 1, 0, 0).action_held(A_Jump) &&
                m.resolve(pad_with(0, c::PadA), 1, 0, 0).held == 0;
    return two_pads && same_slot && kept;
}

static bool test_mouse_refused() {
    ActionMap m;
    ActionLayout mouse = left_hand();
    mouse.bind(A_Punch, {SourceKind::MouseButton, c::MLeft, 1});
    mouse.bind_axis(AX_AimX, {SourceKind::MouseAxis, c::MAxX, 1}, {}, fix32{});
    if (!one_keyboard_map(m) || !m.set_layout(0, mouse)) return false;
    ActionLayout click = right_hand(), look = right_hand();
    click.bind(A_Punch, {SourceKind::MouseButton, c::MLeft, 1});
    look.bind_axis(AX_AimX, {SourceKind::MouseAxis, c::MAxX, -1}, {}, fix32{});
    SharedInput b, a;
    return !m.set_layout(1, click, &b) && refused_names(b, 1, 0, SourceKind::MouseButton, c::MLeft, -1) &&
           !m.set_layout(1, look, &a) && refused_names(a, 1, 0, SourceKind::MouseAxis, c::MAxX, -1);
}

static bool test_pad_axis_sign_is_same_input() {
    ActionMap m;
    ActionLayout up, down;
    up.bind_axis(AX_MoveY, {SourceKind::PadAxis, c::LY, 1}, {}, fix32{});
    down.bind_axis(AX_MoveY, {SourceKind::PadAxis, c::LY, -1}, {}, fix32{});
    SharedInput sh;
    return m.set_layout(0, up) && m.assign_player(0, pad(1)) && m.assign_player(1, pad(1)) &&
           !m.set_layout(1, down, &sh) && refused_names(sh, 1, 0, SourceKind::PadAxis, c::LY, 1);
}

static bool test_pad_source_without_pad() {
    ActionMap m;
    ActionLayout with_pad = left_hand(), stray = right_hand();
    with_pad.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    stray.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    const PlayerAssign both{.pad_slot = 0, .use_kbd_mouse = true};
    return m.set_layout(0, with_pad) && m.assign_player(0, both) && m.set_layout(1, stray) &&
           m.assign_player(1, keyboard()) && m.resolve(pad_with(0, c::PadA), 0, 0, 0).action_held(A_Jump) &&
           m.resolve(pad_with(0, c::PadA), 1, 0, 0).held == 0;
}

static bool test_lone_axes_share_nothing() {
    ActionMap m;
    ActionLayout l = left_hand(), r = right_hand();
    l.bind_axis(AX_AimX, {SourceKind::Key, KEY_F, 1}, {}, fix32{});
    r.bind_axis(AX_AimX, {SourceKind::Key, KEY_G, 1}, {}, fix32{});
    return m.set_layout(0, l) && m.set_layout(1, r) && m.assign_player(0, keyboard()) && m.assign_player(1, keyboard());
}

static bool empty_frame(const InputFrame& f, uint32_t tick) {
    if (f.tick != tick || f.held != 0 || f.pressed != 0 || f.released != 0) return false;
    for (const fix32& a : f.axes) { if (!(a == fix32{})) return false; }
    return true;
}

static bool test_no_fallback_to_player0() {
    ActionMap m;
    if (!one_keyboard_map(m)) return false;
    DeviceState d = keys_down({c::Space, c::D});
    bool p0_reads = m.resolve(d, 0, 5, 0).action_held(A_Jump);
    bool outside = empty_frame(m.resolve(d, -1, 5, ~0ull), 5) && empty_frame(m.resolve(d, MAX_PLAYERS, 5, ~0ull), 5);
    bool refused = !m.set_layout(MAX_PLAYERS, ActionLayout{}) && !m.assign_player(-1, PlayerAssign{});
    return p0_reads && outside && refused && m.resolve(d, 0, 6, 0).action_held(A_Jump);
}

int main() {
    struct { const char* n; bool ok; } t[] = {
        {"one keyboard, two layouts", test_one_keyboard()},
        {"shared key refused", test_shared_key_refused()},
        {"key-as-axis is a key", test_key_as_axis_counts()},
        {"context does not split", test_context_does_not_split()},
        {"assignment refused", test_assignment_refused()},
        {"pad slots", test_pad_slots()},
        {"mouse refused", test_mouse_refused()},
        {"pad axis sign is one input", test_pad_axis_sign_is_same_input()},
        {"pad source without a pad", test_pad_source_without_pad()},
        {"lone axes share nothing", test_lone_axes_share_nothing()},
        {"no fallback to player 0", test_no_fallback_to_player0()},
    };
    printf("input-layouts gate:\n");
    bool all = true;
    for (auto& x : t) { printf("  %-28s %s\n", x.n, x.ok ? "YES" : "NO"); all &= x.ok; }
    printf("%s\n", all ? "input-layouts: PASS - 11 cases" : "input-layouts: FAIL");
    return all ? 0 : 1;
}
