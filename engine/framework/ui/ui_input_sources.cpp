#include "ui_input_sources.hpp"

#include <algorithm>
#include <iterator>

#include "ui_input.hpp"

namespace framework::ui {
bool detail::ui_down(const ::input::Source& s, const ::input::DeviceState& d) {
    using ::input::SourceKind;
    if (s.kind == SourceKind::Key) return d.key_down(s.code);
    if (s.kind == SourceKind::MouseButton) return d.mouse_down(s.code);
    if (s.kind == SourceKind::TouchButton) return d.touch_down(s.code);
    if (s.kind == SourceKind::PadButton)
        for (int pad = 0; pad < ::input::MAX_DEVICES; ++pad)
            if (d.pad_connected[pad] && d.pad_down(pad, s.code)) return true;
    return false;
}
fix32 detail::ui_axis(const ::input::Source& s, const ::input::DeviceState& d, int pad) {
    using ::input::SourceKind;
    int32_t raw = 0;
    if (pad >= 0) {
        if (s.kind == SourceKind::PadAxis && d.pad_connected[pad]) raw = d.pad_axis(pad, s.code).raw;
    } else if (s.kind == SourceKind::Key) raw = d.key_down(s.code) ? fix32::ONE : 0;
    else if (s.kind == SourceKind::TouchAxis) raw = d.touch_axis(s.code).raw;
    else if (s.kind == SourceKind::MouseAxis) {
        if (s.code == 0) raw = d.frame_dx.raw;
        if (s.code == 1) raw = d.frame_dy.raw;
        if (s.code == 2) raw = d.frame_wheel.raw;
    }
    // Native sources ограничены, но чужой RawEvent также не должен переполнить sign*INT_MIN.
    return fix32::from_raw(std::clamp(raw, -fix32::ONE, fix32::ONE) * s.sign);
}
void UiInputChannel::keyboard(const ::input::DeviceState& d, UiInput& out, bool emit) {
    const bool shift = d.key_down(340) || d.key_down(344);
    for (int code = 32; code < ::input::KBD_KEYS; ++code) {
        const bool pressed = d.key_down(code) && !(previous_keys_[code / 64] & (uint64_t{1} << (code % 64)));
        if (!emit || !pressed || out.character_count == out.characters.size()) continue;
        uint32_t cp = 0;
        if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9') || code == '.' || code == ' ') cp = code;
        if (code == ';') cp = shift ? ':' : ';';
        if (code >= 320 && code <= 329) cp = '0' + code - 320;
        if (code == 330) cp = '.';
        if (cp) out.characters[out.character_count++] = cp;
    }
    std::copy(std::begin(d.keys), std::end(d.keys), previous_keys_.begin());
}
}
