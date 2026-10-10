#include "action_shared.hpp"

namespace input {
namespace {

bool reaches(const Source& s, const PlayerAssign& pa) {
    switch (s.kind) {
    case SourceKind::Key:
    case SourceKind::MouseButton:
    case SourceKind::MouseAxis: return pa.use_kbd_mouse;
    case SourceKind::PadButton:
    case SourceKind::PadAxis:   return pa.pad_slot >= 0;
    default: return false;
    }
}

bool same_input(const Source& a, const PlayerAssign& pa, const Source& b, const PlayerAssign& pb) {
    if (!reaches(a, pa) || !reaches(b, pb) || a.kind != b.kind || a.code != b.code) return false;
    const bool pad = a.kind == SourceKind::PadButton || a.kind == SourceKind::PadAxis;
    return !pad || pa.pad_slot == pb.pad_slot;
}

template <class F>
void each_source(const ActionLayout& l, F&& f) {
    for (const ActionBinding& b : l.buttons()) f(b.src);
    for (const AxisBinding& b : l.axes()) {
        f(b.pos);
        f(b.neg);
    }
}

} // namespace

bool find_shared_input(const ActionLayout& a, const PlayerAssign& pa, const ActionLayout& b,
                       const PlayerAssign& pb, Source& out) {
    bool found = false;
    each_source(a, [&](const Source& sa) {
        each_source(b, [&](const Source& sb) {
            if (found || !same_input(sa, pa, sb, pb)) return;
            out = sa;
            found = true;
        });
    });
    return found;
}

} // namespace input
