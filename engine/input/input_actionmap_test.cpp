#include <cstdio>
#include "action_map.hpp"
#include "codes.hpp"
#include "input_engine.hpp"
#include "input_sim.hpp"

// Прямая валидация «Full»-слоя действий: контексты+consume, rebind (listen-next), per-player
// device assignment, input-buffer leniency, capture_source. Deviceless, гоняется в CI.
using namespace input;
namespace c = input::code;

enum { CTX_GAMEPLAY = 1, CTX_MENU = 2 };
enum { A_MenuSelect = 2 };

static DeviceState kbd_with(int key) { DeviceState d; d.keys[key >> 6] |= (1ull << (key & 63)); return d; }
static DeviceState pad_with(int slot, int btn) { DeviceState d; d.pad_connected[slot] = true; d.pad_btns[slot] |= (1u << btn); return d; }

static bool test_context_consume() {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::Key, c::Space, 1}, CTX_GAMEPLAY);
    l.bind(A_MenuSelect, {SourceKind::Key, c::Enter, 1}, CTX_MENU);
    ActionMap m;
    PlayerAssign pa; pa.use_kbd_mouse = true;
    bool set = m.set_layout(0, l) && m.assign_player(0, pa);
    DeviceState both; both.keys[c::Space >> 6] |= (1ull << (c::Space & 63)); both.keys[c::Enter >> 6] |= (1ull << (c::Enter & 63));

    m.push_context(CTX_GAMEPLAY, false);
    bool jump_ok = m.resolve(both, 0, 0, 0).action_held(A_Jump);
    m.push_context(CTX_MENU, true); // consume → нижний Gameplay блокируется
    InputFrame f = m.resolve(both, 0, 1, 0);
    bool menu_ok = f.action_held(A_MenuSelect) && !f.action_held(A_Jump);
    m.pop_context();
    bool restored = m.resolve(both, 0, 2, 0).action_held(A_Jump);
    return set && jump_ok && menu_ok && restored;
}

static bool test_rebind() {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    ActionMap m;
    PlayerAssign pa; pa.use_kbd_mouse = true;
    bool set = m.set_layout(0, l) && m.assign_player(0, pa);
    bool before = m.resolve(kbd_with(c::Space), 0, 0, 0).action_held(A_Jump);
    // listen-next: игрок нажал K → захватываем источник и перебиндиваем.
    Source s = capture_source({RawKind::KeyDown, DeviceKind::Keyboard, 0, static_cast<uint16_t>(c::S), 0, 0});
    l.rebind(A_Jump, 0, s);
    bool reset = m.set_layout(0, l);
    bool now_k = m.resolve(kbd_with(c::S), 0, 1, 0).action_held(A_Jump);
    bool old_gone = !m.resolve(kbd_with(c::Space), 0, 2, 0).action_held(A_Jump);
    return set && reset && before && now_k && old_gone;
}

static bool test_per_player() {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    ActionMap m;
    PlayerAssign p0; p0.pad_slot = 0;
    PlayerAssign p1; p1.pad_slot = 1;
    bool set = m.set_layout(0, l) && m.set_layout(1, l) && m.assign_player(0, p0) && m.assign_player(1, p1);
    DeviceState d = pad_with(0, c::PadA); // нажат pad слота 0
    bool p0_yes = m.resolve(d, 0, 0, 0).action_held(A_Jump);
    bool p1_no = !m.resolve(d, 1, 0, 0).action_held(A_Jump);
    DeviceState d1 = pad_with(1, c::PadA);
    bool p1_yes = m.resolve(d1, 1, 0, 0).action_held(A_Jump);
    return set && p0_yes && p1_no && p1_yes;
}

static bool test_buffer_leniency() {
    ActionLayout l;
    l.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    ActionMap m;
    PlayerAssign pa; pa.use_kbd_mouse = true;
    if (!m.set_layout(0, l) || !m.assign_player(0, pa)) return false;
    InputEngine e(m);
    e.post({RawKind::KeyDown, DeviceKind::Keyboard, 0, c::Space, 0, 0});
    e.begin_tick(0);                                    // pressed @tick0
    e.post({RawKind::KeyUp, DeviceKind::Keyboard, 0, c::Space, 0, 1});
    for (uint32_t t = 1; t <= 3; ++t) e.begin_tick(t);  // отпущено, 3 тика прошло
    bool within = e.buffer().pressed_within(A_Jump, 5); // ещё в окне 5
    for (uint32_t t = 4; t <= 8; ++t) e.begin_tick(t);  // окно прошло
    bool expired = !e.buffer().pressed_within(A_Jump, 5);
    return within && expired;
}

int main() {
    struct { const char* n; bool ok; } t[] = {
        {"context+consume", test_context_consume()},
        {"rebind (listen-next)", test_rebind()},
        {"per-player device assign", test_per_player()},
        {"input-buffer leniency", test_buffer_leniency()},
    };
    printf("input-actionmap gate:\n");
    bool all = true;
    for (auto& x : t) { printf("  %-28s %s\n", x.n, x.ok ? "YES" : "NO"); all &= x.ok; }
    printf("%s\n", all ? "input-actionmap: PASS" : "input-actionmap: FAIL");
    return all ? 0 : 1;
}
