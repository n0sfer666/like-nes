#pragma once

namespace input {

// Назначение устройств игроку: pad-слот и/или клавиатура с мышью.
struct PlayerAssign {
    int pad_slot = -1;         // -1 = нет геймпада
    bool use_kbd_mouse = false;
};

} // namespace input
