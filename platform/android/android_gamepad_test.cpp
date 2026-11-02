#include "android_gamepad.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "android_pad_device.hpp"
#include "codes.hpp"
#include "lobby.hpp"
#include "pad_registry.hpp"
#include "preset_bake.hpp"

// Настоящие NDK-заголовки, backend и InputEngine; заменены лишь события/инвентарь ОС.
struct AInputEvent {
    int32_t type = AINPUT_EVENT_TYPE_KEY, source = AINPUT_SOURCE_GAMEPAD, id = 12;
    int32_t action = AKEY_EVENT_ACTION_DOWN, code = AKEYCODE_BUTTON_X, flags = 0;
    size_t pointers = 1;
    std::array<float, 64> axes{};
};
extern "C" int32_t AInputEvent_getType(const AInputEvent* e) { return e->type; }
extern "C" int32_t AInputEvent_getSource(const AInputEvent* e) { return e->source; }
extern "C" int32_t AInputEvent_getDeviceId(const AInputEvent* e) { return e->id; }
extern "C" int32_t AKeyEvent_getAction(const AInputEvent* e) { return e->action; }
extern "C" int32_t AKeyEvent_getKeyCode(const AInputEvent* e) { return e->code; }
extern "C" int32_t AKeyEvent_getFlags(const AInputEvent* e) { return e->flags; }
extern "C" int32_t AMotionEvent_getAction(const AInputEvent* e) { return e->action; }
extern "C" size_t AMotionEvent_getPointerCount(const AInputEvent* e) { return e->pointers; }
extern "C" float AMotionEvent_getAxisValue(const AInputEvent* e, int32_t axis, size_t pointer) {
    if (pointer >= e->pointers) std::abort();
    return e->axes.at(static_cast<size_t>(axis));
}

namespace {
std::vector<int> live{12, 44, 90};
std::map<int, int> metadata;
bool inventory_ok = true;
int fails;
int rumble_id = -1, rumble_ms = 0;
float rumble_strength = 0;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("android-pad: FAIL %s\n", why); } }
}

bool android_pad_ids(JavaVM*, std::array<jint, 128>& ids, size_t& count) {
    if (!inventory_ok) return false;
    count = live.size();
    std::copy(live.begin(), live.end(), ids.begin());
    return true;
}
bool android_pad_device(JavaVM*, int id, AndroidPadDevice& out) {
    ++metadata[id];
    out = {};
    out.gamepad = id != 90;
    out.info.vid = 0x045e; out.info.pid = 0x0b12;
    std::strcpy(out.info.name, "fixture Xbox");
    out.rx = AMOTION_EVENT_AXIS_Z; out.ry = AMOTION_EVENT_AXIS_RZ;
    return true;
}
bool android_pad_rumble(JavaVM*, int id, float strength, int ms) {
    rumble_id = id; rumble_strength = strength; rumble_ms = ms;
    return true;
}

int main() {
    namespace c = input::code;
    JavaVM vm{};
    android_gamepad_begin(&vm);
    auto* pad = input::make_gamepad_source();
    check(pad->init(), "backend initialized on the game thread");
    input::ActionMap map;
    input::InputEngine engine(map);
    framework::input::Lobby lobby(map, 2);
    const auto poll = [&] { pad->poll(engine); engine.drain(); lobby.update(engine.device()); };
    poll();
    check(engine.device().pad_connected[0] && engine.device().pad_connected[1] &&
          !engine.device().pad_connected[2], "cold scan connects only gamepads in stable slots");
    check(lobby.join(0, {.pad_slot = 0}), "P1 owns the first controller");
    check(pad->pad_info(0).vid == 0x045e && pad->pad_info(0).pid == 0x0b12, "passport reaches the slot");
    pad->set_rumble(1, .25f, .75f, 30);
    check(rumble_id == 44 && rumble_strength == .75f && rumble_ms == 30, "rumble uses the correct physical id");
    pad->set_rumble(-1, 1, 1, 30);
    check(rumble_id == 44, "invalid rumble slot is ignored");
    std::vector<uint8_t> blob;
    framework::input::PresetBakeError error;
    check(framework::input::bake_presets("preset | default\naction | jump | pad:south\n"
        "pad | fixture Xbox | 0x045e | 0x0b12 | Xbox | xbox | 0.18 | 0.12\n", blob, error), "profile fixture bakes");
    framework::input::PresetTable table;
    check(table.open(blob.data(), blob.size()), "profile fixture opens");
    framework::input::PadRegistry registry;
    registry.connected(0, pad->pad_info(0), table);
    check(std::strcmp(registry.profile(0).name, "fixture Xbox") == 0, "actual slot pad_info selects PadRegistry profile");
    android_gamepad_refresh(); poll();
    check(metadata[12] == 1 && metadata[44] == 1, "VID/PID read once per connection");
    AInputEvent event;
    check(android_gamepad_event(&event) == 1, "gamepad key routed"); poll();
    check(engine.device().pad_down(0, c::PadX), "Android BUTTON_X is normalized Xbox X");
    event.id = 44; event.code = AKEYCODE_BUTTON_A;
    (void)android_gamepad_event(&event); poll();
    check(engine.device().pad_down(1, c::PadA) && !engine.device().pad_down(0, c::PadA), "two devices stay independent");
    event.id = 12; event.code = AKEYCODE_DPAD_UP;
    event.source = AINPUT_SOURCE_DPAD;
    check(android_gamepad_event(&event) == 1, "DPAD source accepted for a confirmed gamepad");
    event.type = AINPUT_EVENT_TYPE_MOTION; event.source = AINPUT_SOURCE_JOYSTICK; event.action = AMOTION_EVENT_ACTION_MOVE;
    event.axes[AMOTION_EVENT_AXIS_X] = .5f; event.axes[AMOTION_EVENT_AXIS_Y] = -.5f;
    event.axes[AMOTION_EVENT_AXIS_Z] = -.25f; event.axes[AMOTION_EVENT_AXIS_RZ] = .25f;
    event.axes[AMOTION_EVENT_AXIS_BRAKE] = .75f; event.axes[AMOTION_EVENT_AXIS_GAS] = 1.f;
    event.axes[AMOTION_EVENT_AXIS_HAT_Y] = -1;
    (void)android_gamepad_event(&event); poll();
    check(engine.device().pad_axis(0, c::LX) == fix32::from_float(.5f) &&
          engine.device().pad_axis(0, c::LY) == fix32::from_float(-.5f), "NDK Y already points down");
    check(engine.device().pad_axis(0, c::RX) == fix32::from_float(-.25f) &&
          engine.device().pad_axis(0, c::RT) == fix32::from_int(1), "right stick and alternate triggers");
    event.axes[AMOTION_EVENT_AXIS_HAT_Y] = 0;
    event.axes[AMOTION_EVENT_AXIS_X] = std::nanf(""); event.axes[AMOTION_EVENT_AXIS_Y] = 99;
    (void)android_gamepad_event(&event); poll();
    check(engine.device().pad_down(0, c::DpUp), "hat release does not release the key channel");
    check(engine.device().pad_axis(0, c::LX).raw == 0 && engine.device().pad_axis(0, c::LY) == fix32::from_int(1),
          "nonfinite and out-of-range axes sanitized before fix32");
    event.type = AINPUT_EVENT_TYPE_KEY; event.action = AKEY_EVENT_ACTION_UP;
    (void)android_gamepad_event(&event); poll();
    check(!engine.device().pad_down(0, c::DpUp), "key release completes dpad release");
    event.source = AINPUT_SOURCE_GAMEPAD; event.code = AKEYCODE_BUTTON_L2; event.action = AKEY_EVENT_ACTION_DOWN;
    check(android_gamepad_event(&event) == 1, "digital-only trigger accepted"); poll();
    check(engine.device().pad_axis(0, c::LT) == fix32::from_int(1), "digital trigger overrides partial analog");
    event.type = AINPUT_EVENT_TYPE_MOTION; event.source = AINPUT_SOURCE_JOYSTICK; event.action = AMOTION_EVENT_ACTION_MOVE;
    event.axes[AMOTION_EVENT_AXIS_BRAKE] = .5f;
    (void)android_gamepad_event(&event); poll();
    event.type = AINPUT_EVENT_TYPE_KEY; event.source = AINPUT_SOURCE_GAMEPAD; event.action = AKEY_EVENT_ACTION_UP;
    (void)android_gamepad_event(&event); poll();
    check(engine.device().pad_axis(0, c::LT) == fix32::from_float(.5f), "digital release preserves analog trigger");
    event.id = 90; event.source = AINPUT_SOURCE_DPAD; event.code = AKEYCODE_DPAD_UP;
    check(android_gamepad_event(&event) == 0, "DPAD-only remote does not become a controller");
    event.source = AINPUT_SOURCE_KEYBOARD;
    check(android_gamepad_event(&event) == 0, "keyboard/touch never captured as a pad");
    pad->set_rumble(1, .5f, .5f, 60000); // Мотор замокан; длинный срок исключает зависимость от скорости CI.
    android_gamepad_cancel(); poll();
    check(rumble_ms == 0 && rumble_strength == 0, "focus cancel stops this client's active rumble");
    check(engine.device().pad_btns[0] == 0 && engine.device().pad_axis(0, c::LY).raw == 0, "focus cancel releases held");
    live = {44, 90, 77}; android_gamepad_refresh(); poll();
    check(!engine.device().pad_connected[0] && engine.device().pad_connected[1], "silent unplug discovered without an input event");
    check(lobby.lost(0) && lobby.paused(), "replacement id cannot silently take the old player's slot");
    registry.disconnected(0);
    check(!registry.active(0) && std::strcmp(registry.profile(0).name, "generic") == 0, "unplug profile returns to generic");
    inventory_ok = false; android_gamepad_refresh(); poll();
    check(!engine.device().pad_connected[1] && engine.device().pad_btns[1] == 0, "JNI inventory failure cannot leave held buttons");
    inventory_ok = true; android_gamepad_refresh(); poll();
    check(engine.device().pad_connected[0] && metadata[44] == 2, "reconnect obtains a fresh passport");
    android_gamepad_end();
    check(!pad->init() && android_gamepad_event(&event) == 0 && engine.dropped() == 0, "teardown disables routing");
    std::puts(fails ? "android-pad: FAIL" : "android-pad: PASS native events, metadata cache, hotplug, cancel and JNI failure");
    return fails ? 1 : 0;
}
