#pragma once

namespace input {

// Назначение устройств игроку: pad-слот, клавиатура с мышью и/или экранный тач. Собирать по именам
// полей: позиционная сборка из двух значений молча обнуляет use_touch.
struct PlayerAssign {
    int pad_slot = -1;         // -1 = нет геймпада
    bool use_kbd_mouse = false;
    bool use_touch = false;
};

} // namespace input
