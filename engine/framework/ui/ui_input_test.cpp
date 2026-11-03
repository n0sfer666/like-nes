#include "ui_input.hpp"
#include "ui_input_fixture.hpp"

#include <cstdio>
#include <limits>

#include "action_map.hpp"
#include "codes.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "preset_bake.hpp"

namespace {
int fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("ui-input: FAIL %s\n", why); } }
void key(::input::DeviceState& d, int code, bool down) {
    d.apply({down ? ::input::RawKind::KeyDown : ::input::RawKind::KeyUp, ::input::DeviceKind::Keyboard, 0, static_cast<uint16_t>(code)});
}
}
int main() {
    using namespace framework::ui;
    using ::input::RawKind;
    std::vector<uint8_t> bytes; framework::input::PresetBakeError error;
    framework::input::PresetTable table;
    check(framework::input::bake_presets(ui_input_fixture::PRESET, bytes, error) && table.open(bytes.data(), bytes.size()), "actual baked ui preset");
    UiInputChannel channel;
    check(channel.init(table), "named ui channel configured outside frame");
    table = {}; bytes.clear(); bytes.shrink_to_fit(); // канал хранит копию биндингов, не view таблицы.
    ::input::DeviceState devices;
    key(devices, 264, true);
    check(channel.read(devices, 0).y == 1, "keyboard navigation edge");
    check(channel.read(devices, 349).y == 0 && channel.read(devices, 1).y == 1 &&
          channel.read(devices, 89).y == 0 && channel.read(devices, 1).y == 1, "350/90 repeat without OS repeat events");
    key(devices, 265, true);
    check(channel.read(devices, 1000).y == 0, "opposing directions neutral");
    key(devices, 264, false);
    check(channel.read(devices, UINT32_MAX).y == -1, "direction change immediate, large delta bounded");
    devices.reset_all_held(); channel.read(devices, 0);
    for (uint8_t pad = 0; pad < ::input::MAX_DEVICES; ++pad) {
        devices.apply({RawKind::DeviceConnected, ::input::DeviceKind::Gamepad, pad});
        devices.apply({RawKind::PadButtonDown, ::input::DeviceKind::Gamepad, pad, ::input::code::PadA});
        check(channel.read(devices, 0).accept && !channel.read(devices, 0).accept, "every pad, one edge per held action");
        devices.apply({RawKind::DeviceDisconnected, ::input::DeviceKind::Gamepad, pad});
        check(!channel.read(devices, 0).accept, "unplug releases UI action");
    }
    devices.pad_btns[7] = 1;
    check(!channel.read(devices, 0).accept, "unconnected stale pad ignored");
    devices.pad_btns[7] = 0; devices.pad_connected[7] = true;
    devices.apply({RawKind::PadButtonDown, ::input::DeviceKind::Gamepad, 7, ::input::code::Start});
    check(channel.read(devices, 0).done && !channel.read(devices, 1000).done, "optional done action is edge-triggered on Start");
    channel.cancel(); channel.read(devices, 0);
    check(!channel.read(devices, 1000).done, "cancel blocks held Start");
    devices.apply({RawKind::PadButtonUp, ::input::DeviceKind::Gamepad, 7, ::input::code::Start}); channel.read(devices, 0);
    devices.pad_axes[7][0] = fix32::from_raw(fix32::ONE / 4);
    check(channel.read(devices, 0).x == 0, "preset deadzone");
    devices.pad_axes[7][0] = fix32::from_int(1);
    check(channel.read(devices, 0).x == 1, "last pad stick independent of assignment");
    channel.cancel();
    check(channel.read(devices, 0).cancelled && channel.read(devices, 1000).x == 0, "cancel blocks held analog navigation until neutral");
    devices.pad_axes[7][0] = {}; channel.read(devices, 0);
    devices.pad_axes[7][0] = fix32::from_raw(INT32_MIN);
    check(channel.read(devices, 0).x == -1, "hostile raw axis clamped without signed overflow");
    devices.reset_all_held(); channel.read(devices, 0);

    ::input::ActionMap game_map; ::input::ActionLayout game_layout;
    game_layout.bind(0, {::input::SourceKind::Key, 257});
    check(game_map.set_layout(0, std::move(game_layout)) && game_map.assign_player(0, {.pad_slot = 7, .use_kbd_mouse = true}), "game claims keyboard and pad");
    key(devices, 257, true);
    const auto before = game_map.resolve(devices, 0, 1, 0);
    check(channel.read(devices, 0).accept && game_map.resolve(devices, 0, 1, 0).pressed == before.pressed &&
          before.action_pressed(0), "UI does not consume/reassign game input");
    channel.cancel();
    check(channel.read(devices, 0).cancelled && !channel.read(devices, 0).accept, "cancel blocks held activation until release");
    key(devices, 257, false); channel.read(devices, 0); key(devices, 257, true);
    check(channel.read(devices, 0).accept, "fresh activation after release");
    key(devices, 257, false); key(devices, 259, true);
    check(channel.read(devices, 0).erase && !channel.read(devices, 349).erase && channel.read(devices, 1).erase, "erase repeats");
    channel.cancel(); channel.read(devices, 0);
    check(!channel.read(devices, 1000).erase, "cancel cannot restart held destructive erase");
    devices.reset_all_held(); channel.read(devices, 0);
    key(devices, 'A', true); key(devices, '3', true); key(devices, '.', true); key(devices, ';', true); key(devices, 340, true);
    auto frame = channel.read(devices, 0);
    check(frame.character_count == 4 && frame.characters[0] == '.' && frame.characters[1] == '3' &&
          frame.characters[2] == ':' && frame.characters[3] == 'A' && channel.read(devices, 0).character_count == 0,
          "physical Latin keyboard, digits, punctuation and edges");
    const uint32_t unicode[]{0x416, 0, 0xd800, 0x80, 0x110000, 'Z'};
    frame = channel.read(devices, 0, {}, unicode, UiTextSource::Native);
    check(frame.character_count == 2 && frame.characters[0] == 0x416 && frame.characters[1] == 'Z', "native Unicode filters invalid/control scalars, no physical duplicates");
    const UiPointer pointers[]{{INTPTR_MAX, UiPointerPhase::Down, 10, 20}, {INTPTR_MIN, UiPointerPhase::Up, -1, 30}};
    frame = channel.read(devices, 0, pointers);
    check(frame.pointer_count == 2 && frame.pointers[0].id == INTPTR_MAX && frame.pointers[1].id == INTPTR_MIN &&
          frame.pointers[1].x == -1, "zone coordinates and full-width pointer identity preserved");
    std::array<UiPointer, UI_POINTERS + 1> overflow{};
    check(channel.read(devices, 0, overflow).cancelled, "pointer overflow cancels, never loses a release silently");
    const UiPointer bad{1, UiPointerPhase::Move, std::numeric_limits<float>::quiet_NaN(), 0};
    check(channel.read(devices, 0, {&bad, 1}).cancelled, "nonfinite coordinate cancels");
    std::array<uint32_t, UI_CHARACTERS + 1> huge_text{};
    check(channel.read(devices, 0, {}, huge_text, UiTextSource::Native).cancelled, "native text overflow cancels");

    framework::probe::in_hot = true;
    const bool control = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(control && framework::probe::allocs >= 2, "allocation counter sees real allocation");
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    for (int i = 0; i < 1000; ++i) { frame = channel.read(devices, 16, pointers, unicode, UiTextSource::Native); channel.cancel(); }
    framework::probe::in_hot = false;
    check(framework::probe::allocs == 0, "read/cancel use fixed buffers with zero allocations");
    auto custom = ui_input_fixture::PRESET;
    const std::string original = "action|right|key:right|pad:dpright";
    custom.replace(custom.find(original), original.size(), "action|right|key:h|pad:dpright");
    check(framework::input::bake_presets(custom, bytes, error) && table.open(bytes.data(), bytes.size()) && channel.init(table), "rebaked custom preset");
    devices.reset_all_held(); key(devices, 262, true);
    check(channel.read(devices, 0).x == 0, "removed default binding is not hardcoded");
    key(devices, 'H', true);
    check(channel.read(devices, 0).x == 1, "custom preset binding reaches UI");
    custom = ui_input_fixture::PRESET;
    for (const char* axis : {"x", "y"}) {
        const std::string old = std::string("shape|") + axis + "|0.25|1.0|1|-";
        custom.replace(custom.find(old), old.size(), std::string("shape|") + axis + "|0.4|1.0|1|" + (axis[0] == 'x' ? "y" : "x"));
    }
    check(framework::input::bake_presets(custom, bytes, error) && table.open(bytes.data(), bytes.size()) && channel.init(table), "paired axes preset");
    devices.reset_all_held(); devices.pad_connected[7] = true;
    devices.pad_axes[7][0] = devices.pad_axes[7][1] = fix32::from_raw(39322); // .6,.6: radial passes, axial would not.
    frame = channel.read(devices, 0);
    check(frame.x == 1 && frame.y == 1, "paired preset uses a radial response before navigation thresholds");
    check(framework::input::bake_presets(ui_input_fixture::PRESET + "axis|x|key:d|key:a\n", bytes, error) &&
          table.open(bytes.data(), bytes.size()) && channel.init(table), "keyboard-as-axis preset");
    devices.reset_all_held(); key(devices, 'D', true);
    check(channel.read(devices, 0).x == 1, "key-as-axis navigation");
    channel.cancel(); channel.read(devices, 0);
    check(channel.read(devices, 1000).x == 0, "cancel blocks held key-as-axis");
    key(devices, 'D', false); channel.read(devices, 0); key(devices, 'A', true);
    check(channel.read(devices, 0).x == -1, "key-as-axis rearms after release");
    key(devices, 'D', true);
    check(channel.read(devices, 0).x == 0, "opposing keys in one binding cancel");
    channel.cancel(); channel.read(devices, 0);
    key(devices, 'A', false);
    check(channel.read(devices, 1000).x == 0, "cancel tracks source activity before D minus A");
    key(devices, 'D', false); channel.read(devices, 0); key(devices, 'D', true);
    check(channel.read(devices, 0).x == 1, "same-binding cancel rearms only after release");
    check(framework::input::bake_presets(ui_input_fixture::PRESET + "axis|x|padaxis:rx|-\n", bytes, error) &&
          table.open(bytes.data(), bytes.size()) && channel.init(table), "alternative pad axes");
    devices.reset_all_held(); devices.pad_connected[7] = true;
    devices.pad_axes[7][0] = fix32::from_int(1); devices.pad_axes[7][2] = fix32::from_int(-1);
    check(channel.read(devices, 0).x == 0, "opposing alternatives on one pad cancel");
    channel.cancel(); channel.read(devices, 0); devices.pad_axes[7][2] = {};
    check(channel.read(devices, 1000).x == 0, "cancel tracks alternatives before per-pad sum");
    devices.reset_all_held(); channel.read(devices, 0); devices.pad_connected[0] = devices.pad_connected[7] = true;
    devices.pad_axes[0][0] = fix32::from_int(1); devices.pad_axes[7][0] = fix32::from_int(-1);
    check(channel.read(devices, 0).x == 0, "opposing pads neutral");
    channel.cancel(); channel.read(devices, 0);
    devices.apply({RawKind::DeviceDisconnected, ::input::DeviceKind::Gamepad, 7});
    check(channel.read(devices, 1000).x == 0, "cancel latches opposing active sources until neutral, not their zero sum");
    custom = ui_input_fixture::PRESET;
    const std::string axis_source = "padaxis:lx";
    custom.replace(custom.find(axis_source), axis_source.size(), "padaxis:-lx");
    check(framework::input::bake_presets(custom, bytes, error) && table.open(bytes.data(), bytes.size()) && channel.init(table), "inverted axis preset");
    devices.reset_all_held(); devices.pad_connected[7] = true; devices.pad_axes[7][0] = fix32::from_raw(INT32_MIN);
    check(channel.read(devices, 0).x == 1, "clamp precedes sign inversion of INT_MIN");
    custom = ui_input_fixture::PRESET + "action|unused";
    for (int i = 0; i < 260; ++i) custom += "|key:a";
    custom += '\n';
    check(framework::input::bake_presets(custom, bytes, error) && table.open(bytes.data(), bytes.size()), "valid high binding count fixture");
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    const bool capped = !channel.init(table);
    framework::probe::in_hot = false;
    check(capped && framework::probe::allocs == 0, "binding cap before layout allocation");
    framework::input::PresetTable absent;
    check(!channel.init(absent) && channel.read(devices, 0).cancelled, "failed reinit clears prior channel");
    std::puts(fails ? "ui-input: FAIL" : "ui-input: PASS all devices, preset, repeat, text, pointers, cancellation and zero allocations");
    return fails ? 1 : 0;
}
