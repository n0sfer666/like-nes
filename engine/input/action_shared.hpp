#pragma once
#include "action_layout.hpp"
#include "player_assign.hpp"

namespace input {

// Игрок, чья раскладка или назначение отвергнуты, другой игрок и вход, который они делили бы.
// pad_slot — слот пада для кнопки или оси пада, иначе -1.
struct SharedInput {
    int player = -1;
    int other = -1;
    Source src;
    int pad_slot = -1;
};

// Источник одной раскладки, который через назначения устройств читает ТОТ ЖЕ физический вход, что
// источник другой: клавиша и мышь — когда оба игрока на клавиатуре, кнопка и ось пада — когда у них
// один слот. Контекст не учитывается: стек общий, и клавиша двух игроков в разных контекстах — та
// же клавиша. Источник, до которого назначение не дотягивается, физическим входом не считается.
bool find_shared_input(const ActionLayout& a, const PlayerAssign& pa, const ActionLayout& b,
                       const PlayerAssign& pb, Source& out);

} // namespace input
