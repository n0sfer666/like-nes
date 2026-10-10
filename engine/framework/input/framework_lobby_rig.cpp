#include "framework_lobby_rig.hpp"

namespace framework::input::test {

bool set_p1_layout(::input::ActionMap& map, int player) {
    ::input::ActionLayout l;
    l.bind(JUMP, key(::input::code::Space));
    l.bind(PUNCH, key(KEY_J));
    l.bind(JUMP, pad(::input::code::PadA));
    l.bind(PUNCH, pad(::input::code::PadX));
    l.bind_axis(0, key(::input::code::D), key(::input::code::A), fix32{});
    return map.set_layout(player, l);
}

bool set_p2_layout(::input::ActionMap& map, int player, uint16_t punch_key) {
    ::input::ActionLayout l;
    l.bind(PUNCH, key(punch_key));
    l.bind(JUMP, pad(::input::code::PadA));
    l.bind(PUNCH, pad(::input::code::PadX));
    l.bind_axis(0, key(KEY_RIGHT), key(KEY_LEFT), fix32{});
    return map.set_layout(player, l);
}

} // namespace framework::input::test
