#include "world.hpp"
#include "codes.hpp"

namespace game {

namespace c = input::code;
using input::Source;
using input::SourceKind;

input::ActionMap make_map() {
    input::ActionMap m;
    const fix32 dz = fix32::from_float(0.18);

    m.bind_axis(AX_MoveX, {SourceKind::Key, c::D, 1}, {SourceKind::Key, c::A, 1}, fix32{});
    m.bind_axis(AX_MoveX, {SourceKind::Key, 262, 1}, {SourceKind::Key, 263, 1}, fix32{});
    m.bind_axis(AX_MoveX, {SourceKind::PadAxis, c::LX, 1}, {SourceKind::None, 0, 0}, dz);
    m.bind_axis(AX_MoveY, {SourceKind::Key, c::W, 1}, {SourceKind::Key, c::S, 1}, fix32{});
    m.bind_axis(AX_MoveY, {SourceKind::Key, 265, 1}, {SourceKind::Key, 264, 1}, fix32{});
    m.bind_axis(AX_MoveY, {SourceKind::PadAxis, c::LY, -1}, {SourceKind::None, 0, 0}, dz);

    m.bind(A_Fire, {SourceKind::Key, c::Space, 1});
    m.bind(A_Fire, {SourceKind::MouseButton, c::MLeft, 1});
    m.bind(A_Fire, {SourceKind::PadButton, c::PadA, 1});

    input::PlayerAssign pa;
    pa.use_kbd_mouse = true;
    pa.pad_slot = 0;
    m.assign_player(0, pa);
    return m;
}

} // namespace game
