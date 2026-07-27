#include "source.hpp"
#include "codes.hpp"
#include <cstring>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

// macOS native gamepad: GameController.framework (Xbox/DualSense/Switch, hot-plug, haptics).
// Диффим состояние против кэша → эмитим только изменения как RawEvent. Слот = порядок открытия.
namespace input {
namespace c = input::code;

namespace {
struct PadCache {
    uintptr_t id = 0;          // идентичность GCController (только сравнение указателя)
    bool active = false;
    uint32_t btns = 0;         // уровни кнопок (наши коды)
    int32_t axes[PAD_AXES] = {};
    CHHapticEngine* haptics = nullptr;
};

class MacGamepadSource : public GamepadSource {
public:
    bool init() override { return true; }

    void poll(InputEngine& e) override {
        NSArray<GCController*>* live = [GCController controllers];
        bool seen[MAX_DEVICES] = {};

        for (GCController* ctl in live) {
            GCExtendedGamepad* gp = ctl.extendedGamepad;
            if (!gp) continue;
            int slot = slot_for(ctl);
            if (slot < 0) continue;
            seen[slot] = true;
            if (!cache_[slot].active) {
                cache_[slot].active = true; cache_[slot].id = (uintptr_t)(__bridge void*)ctl;
                e.post({RawKind::DeviceConnected, DeviceKind::Gamepad, (uint8_t)slot, 0, 0, seq_++});
                pads_[slot] = ctl;
                fill_info(slot, ctl);
            }
            emit_buttons(e, slot, gp);
            emit_axes(e, slot, gp);
        }
        for (int s = 0; s < MAX_DEVICES; ++s)
            if (cache_[s].active && !seen[s]) {
                e.post({RawKind::DeviceDisconnected, DeviceKind::Gamepad, (uint8_t)s, 0, 0, seq_++});
                cache_[s] = PadCache{}; pads_[s] = nil; info_[s] = PadInfo{};
            }
    }

    void set_rumble(int slot, float low, float high, int ms) override {
        if (slot < 0 || slot >= MAX_DEVICES || !pads_[slot]) return;
        float intensity = low > high ? low : high;
        play_haptic(slot, intensity, ms);
    }

    const char* backend_name() const override { return "GameController.framework (macOS)"; }

    // GameController.framework не отдаёт VID/PID вовсе: производителя видно только именем.
    // Профиль ищется по нему, поэтому имя берётся у vendorName, а не у productCategory —
    // категория одинакова у всех extended-падов и различать их не может.
    PadInfo pad_info(int slot) const override {
        if (slot < 0 || slot >= MAX_DEVICES) return {};
        return info_[slot];
    }

private:
    void fill_info(int slot, GCController* ctl) {
        info_[slot] = PadInfo{};
        NSString* n = ctl.vendorName;
        if (n) {
            const char* utf8 = [n UTF8String];
            if (utf8) {
                std::strncpy(info_[slot].name, utf8, sizeof(info_[slot].name) - 1);
                info_[slot].name[sizeof(info_[slot].name) - 1] = '\0';
            }
        }
    }

    int slot_for(GCController* ctl) {
        for (int s = 0; s < MAX_DEVICES; ++s) if (cache_[s].active && cache_[s].id == (uintptr_t)(__bridge void*)ctl) return s;
        for (int s = 0; s < MAX_DEVICES; ++s) if (!cache_[s].active) return s;
        return -1;
    }

    void emit_btn(InputEngine& e, int slot, int code, bool pressed) {
        bool was = (cache_[slot].btns >> code) & 1u;
        if (pressed == was) return;
        if (pressed) cache_[slot].btns |= (1u << code); else cache_[slot].btns &= ~(1u << code);
        e.post({pressed ? RawKind::PadButtonDown : RawKind::PadButtonUp,
                DeviceKind::Gamepad, (uint8_t)slot, (uint16_t)code, 0, seq_++});
    }

    void emit_buttons(InputEngine& e, int slot, GCExtendedGamepad* gp) {
        emit_btn(e, slot, c::PadA, gp.buttonA.pressed); emit_btn(e, slot, c::PadB, gp.buttonB.pressed);
        emit_btn(e, slot, c::PadX, gp.buttonX.pressed); emit_btn(e, slot, c::PadY, gp.buttonY.pressed);
        emit_btn(e, slot, c::LB, gp.leftShoulder.pressed); emit_btn(e, slot, c::RB, gp.rightShoulder.pressed);
        if (gp.buttonMenu) emit_btn(e, slot, c::Start, gp.buttonMenu.pressed);
        if (gp.buttonOptions) emit_btn(e, slot, c::Back, gp.buttonOptions.pressed);
        if (gp.leftThumbstickButton) emit_btn(e, slot, c::LStick, gp.leftThumbstickButton.pressed);
        if (gp.rightThumbstickButton) emit_btn(e, slot, c::RStick, gp.rightThumbstickButton.pressed);
        emit_btn(e, slot, c::DpUp, gp.dpad.up.pressed); emit_btn(e, slot, c::DpDown, gp.dpad.down.pressed);
        emit_btn(e, slot, c::DpLeft, gp.dpad.left.pressed); emit_btn(e, slot, c::DpRight, gp.dpad.right.pressed);
    }

    void emit_axis(InputEngine& e, int slot, int code, float v) {
        int32_t raw = fix32::from_float(v).raw;
        if (raw == cache_[slot].axes[code]) return;
        cache_[slot].axes[code] = raw;
        e.post({RawKind::PadAxis, DeviceKind::Gamepad, (uint8_t)slot, (uint16_t)code, raw, seq_++});
    }

    void emit_axes(InputEngine& e, int slot, GCExtendedGamepad* gp) {
        emit_axis(e, slot, c::LX, gp.leftThumbstick.xAxis.value);
        emit_axis(e, slot, c::LY, gp.leftThumbstick.yAxis.value);
        emit_axis(e, slot, c::RX, gp.rightThumbstick.xAxis.value);
        emit_axis(e, slot, c::RY, gp.rightThumbstick.yAxis.value);
        emit_axis(e, slot, c::LT, gp.leftTrigger.value);
        emit_axis(e, slot, c::RT, gp.rightTrigger.value);
    }

    void play_haptic(int slot, float intensity, int ms) {
        GCController* ctl = pads_[slot];
        if (!ctl.haptics) { NSLog(@"[input] pad slot %d: no haptics (rumble unsupported)", slot); return; }
        NSError* err = nil;
        if (!cache_[slot].haptics) {
            cache_[slot].haptics = [ctl.haptics createEngineWithLocality:GCHapticsLocalityDefault];
            [cache_[slot].haptics startAndReturnError:&err];
        }
        CHHapticEngine* eng = cache_[slot].haptics;
        if (!eng) { NSLog(@"[input] pad slot %d: haptic engine unavailable", slot); return; }
        CHHapticEventParameter* ip = [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:intensity];
        CHHapticEventParameter* sp = [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:0.5f];
        CHHapticEvent* ev = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                              parameters:@[ip, sp] relativeTime:0 duration:(ms / 1000.0)];
        CHHapticPattern* pat = [[CHHapticPattern alloc] initWithEvents:@[ev] parameters:@[] error:&err];
        id<CHHapticPatternPlayer> player = [eng createPlayerWithPattern:pat error:&err];
        [player startAtTime:0 error:&err];
        if (err) NSLog(@"[input] pad slot %d: haptic error %@", slot, err);
    }

    PadCache cache_[MAX_DEVICES];
    PadInfo info_[MAX_DEVICES];
    GCController* pads_[MAX_DEVICES] = {};
    uint64_t seq_ = 1'000'000; // отдельный диапазон seq от kbd/mouse
};
} // namespace

GamepadSource* make_gamepad_source() { static MacGamepadSource s; return &s; }

} // namespace input
