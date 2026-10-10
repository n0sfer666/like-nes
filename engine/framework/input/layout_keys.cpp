#include "layout_keys.hpp"

namespace framework::input {

namespace {

bool key_edge(const ::input::Source& s, const ::input::DeviceState& now, const ::input::DeviceState& prev) {
    return s.kind == ::input::SourceKind::Key && now.key_down(s.code) && !prev.key_down(s.code);
}

} // namespace

bool layout_key_pressed(const ::input::ActionLayout& layout, const ::input::DeviceState& now,
                        const ::input::DeviceState& prev) {
    for (const ::input::ActionBinding& b : layout.buttons())
        if (key_edge(b.src, now, prev)) return true;
    for (const ::input::AxisBinding& a : layout.axes())
        if (key_edge(a.pos, now, prev) || key_edge(a.neg, now, prev)) return true;
    return false;
}

} // namespace framework::input
