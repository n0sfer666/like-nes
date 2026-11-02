#include "android_pad_events.hpp"

#include <algorithm>
#include <cmath>

#include "codes.hpp"

bool android_pad_source(int value) {
    return (value & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD ||
        (value & AINPUT_SOURCE_DPAD) == AINPUT_SOURCE_DPAD ||
        (value & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK;
}

fix32 android_pad_axis(float value, bool trigger) {
    if (!std::isfinite(value)) return {};
    return fix32::from_float(std::clamp(value, trigger ? 0.f : -1.f, 1.f));
}

int android_pad_button(int code) {
    namespace c = input::code;
    switch (code) {
    case AKEYCODE_BUTTON_A: return c::PadA;
    case AKEYCODE_BUTTON_B: return c::PadB;
    case AKEYCODE_BUTTON_X: return c::PadX;
    case AKEYCODE_BUTTON_Y: return c::PadY;
    case AKEYCODE_BUTTON_L1: return c::LB;
    case AKEYCODE_BUTTON_R1: return c::RB;
    case AKEYCODE_BACK: case AKEYCODE_BUTTON_SELECT: return c::Back;
    case AKEYCODE_BUTTON_START: return c::Start;
    case AKEYCODE_BUTTON_THUMBL: return c::LStick;
    case AKEYCODE_BUTTON_THUMBR: return c::RStick;
    case AKEYCODE_DPAD_UP: return c::DpUp;
    case AKEYCODE_DPAD_DOWN: return c::DpDown;
    case AKEYCODE_DPAD_LEFT: return c::DpLeft;
    case AKEYCODE_DPAD_RIGHT: return c::DpRight;
    default: return -1;
    }
}
