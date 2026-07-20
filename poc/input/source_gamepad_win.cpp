#include "source.hpp"
#include "codes.hpp"
#include <windows.h>
#include <Xinput.h>

// Windows native gamepad: XInput (Xbox-класс + вибрация). Диффим против кэша → эмитим изменения.
// Скелет: собирается в CI; live-валидация — follow-up (нет Windows-HW у owner в этом заходе).
namespace input {
namespace c = input::code;

namespace {
struct Btn { WORD mask; int code; };
const Btn kBtns[] = {
    {XINPUT_GAMEPAD_A, c::PadA}, {XINPUT_GAMEPAD_B, c::PadB},
    {XINPUT_GAMEPAD_X, c::PadX}, {XINPUT_GAMEPAD_Y, c::PadY},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, c::LB}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, c::RB},
    {XINPUT_GAMEPAD_BACK, c::Back}, {XINPUT_GAMEPAD_START, c::Start},
    {XINPUT_GAMEPAD_LEFT_THUMB, c::LStick}, {XINPUT_GAMEPAD_RIGHT_THUMB, c::RStick},
    {XINPUT_GAMEPAD_DPAD_UP, c::DpUp}, {XINPUT_GAMEPAD_DPAD_DOWN, c::DpDown},
    {XINPUT_GAMEPAD_DPAD_LEFT, c::DpLeft}, {XINPUT_GAMEPAD_DPAD_RIGHT, c::DpRight},
};

class WinGamepadSource : public GamepadSource {
public:
    bool init() override { return true; }

    void poll(InputEngine& e) override {
        for (int i = 0; i < 4 && i < MAX_DEVICES; ++i) {
            XINPUT_STATE st{};
            bool ok = XInputGetState(i, &st) == ERROR_SUCCESS;
            if (ok && !active_[i]) { active_[i] = true; btns_[i] = 0; for (int a = 0; a < PAD_AXES; ++a) axes_[i][a] = 0; e.post({RawKind::DeviceConnected, DeviceKind::Gamepad, (uint8_t)i, 0, 0, seq_++}); }
            if (!ok && active_[i]) { active_[i] = false; e.post({RawKind::DeviceDisconnected, DeviceKind::Gamepad, (uint8_t)i, 0, 0, seq_++}); continue; }
            if (!ok) continue;
            const XINPUT_GAMEPAD& g = st.Gamepad;
            for (const Btn& b : kBtns) emit_btn(e, i, b.code, (g.wButtons & b.mask) != 0);
            emit_axis(e, i, c::LX, g.sThumbLX / 32768.0f); emit_axis(e, i, c::LY, g.sThumbLY / 32768.0f);
            emit_axis(e, i, c::RX, g.sThumbRX / 32768.0f); emit_axis(e, i, c::RY, g.sThumbRY / 32768.0f);
            emit_axis(e, i, c::LT, g.bLeftTrigger / 255.0f); emit_axis(e, i, c::RT, g.bRightTrigger / 255.0f);
        }
    }

    void set_rumble(int slot, float low, float high, int) override {
        if (slot < 0 || slot >= 4) return;
        XINPUT_VIBRATION v{};
        v.wLeftMotorSpeed = (WORD)(low * 65535.0f);
        v.wRightMotorSpeed = (WORD)(high * 65535.0f);
        XInputSetState(slot, &v);
    }

    const char* backend_name() const override { return "XInput (Windows)"; }

private:
    void emit_btn(InputEngine& e, int slot, int code, bool pressed) {
        bool was = (btns_[slot] >> code) & 1u;
        if (pressed == was) return;
        if (pressed) btns_[slot] |= (1u << code); else btns_[slot] &= ~(1u << code);
        e.post({pressed ? RawKind::PadButtonDown : RawKind::PadButtonUp, DeviceKind::Gamepad, (uint8_t)slot, (uint16_t)code, 0, seq_++});
    }
    void emit_axis(InputEngine& e, int slot, int code, float v) {
        int32_t raw = fix32::from_float(v).raw;
        if (raw == axes_[slot][code]) return;
        axes_[slot][code] = raw;
        e.post({RawKind::PadAxis, DeviceKind::Gamepad, (uint8_t)slot, (uint16_t)code, raw, seq_++});
    }

    bool active_[MAX_DEVICES] = {};
    uint32_t btns_[MAX_DEVICES] = {};
    int32_t axes_[MAX_DEVICES][PAD_AXES] = {};
    uint64_t seq_ = 1'000'000;
};
} // namespace

GamepadSource* make_gamepad_source() { static WinGamepadSource s; return &s; }

} // namespace input
