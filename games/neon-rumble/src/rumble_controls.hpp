#pragma once
#include <cstdint>
#include <string>

#include "action_map.hpp"
#include "rumble_command.hpp"

namespace rumble {

enum Action : int { JUMP, PUNCH, CROSS, KICK, GRAB, BLOCK, DODGE, LEAVE };
enum Axis : int { MOVE_X, MOVE_Z };

enum Key : uint16_t {
    KEY_BACKSPACE = 259, KEY_RIGHT = 262, KEY_LEFT = 263, KEY_DOWN = 264, KEY_UP = 265,
    KEY_H = 72, KEY_I = 73, KEY_J = 74, KEY_K = 75, KEY_L = 76, KEY_O = 79, KEY_U = 85,
    KEY_KP0 = 320, KEY_KP1 = 321, KEY_KP2 = 322, KEY_KP3 = 323, KEY_KP4 = 324, KEY_KP5 = 325,
    KEY_KP6 = 326, KEY_KP_SUBTRACT = 333,
};

// Раскладки ставятся в карту на месте, а не возвращаются значением: GCC 13 -O3 видит в возврате
// ActionLayout запись за буфером (stringop-overflow), которой нет. Отказ карты — текстом в `why`.
[[nodiscard]] bool set_layouts(input::ActionMap& map, std::string& why);

PlayerCommand command_of(const input::InputFrame& f);

} // namespace rumble
