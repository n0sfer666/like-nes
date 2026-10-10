#include "rumble_controls.hpp"

#include "codes.hpp"
#include "player_layout.hpp"
#include "seat.hpp"

namespace rumble {

namespace {

namespace code = input::code;
namespace bb = framework::brawl::button;

const fix32 STICK_DEADZONE = fix32::from_int(3) / fix32::from_int(10);

input::Source key(uint16_t c) { return {input::SourceKind::Key, c, 1}; }
input::Source pad(uint16_t c) { return {input::SourceKind::PadButton, c, 1}; }
input::Source stick(uint16_t c) { return {input::SourceKind::PadAxis, c, 1}; }

// Пад у обоих одинаковый: кнопки пада не общие, пока у игроков разные слоты.
void bind_pad(input::ActionLayout& l) {
    l.bind(JUMP, pad(code::PadA));
    l.bind(PUNCH, pad(code::PadX));
    l.bind(CROSS, pad(code::PadY));
    l.bind(KICK, pad(code::PadB));
    l.bind(GRAB, pad(code::RB));
    l.bind(BLOCK, pad(code::LB));
    l.bind(DODGE, pad(code::RStick));
    l.bind(LEAVE, pad(code::Back));
    l.bind_axis(MOVE_X, stick(code::LX), {}, STICK_DEADZONE);
    l.bind_axis(MOVE_Z, stick(code::LY), {}, STICK_DEADZONE);
}

[[nodiscard]] bool set_p1(input::ActionMap& map, input::SharedInput* shared) {
    input::ActionLayout l;
    bind_pad(l);
    l.bind_axis(MOVE_X, key(code::D), key(code::A), fix32{});
    l.bind_axis(MOVE_Z, key(code::S), key(code::W), fix32{});
    l.bind(JUMP, key(code::Space));
    l.bind(JUMP, key(KEY_K));
    l.bind(PUNCH, key(KEY_J));
    l.bind(CROSS, key(KEY_U));
    l.bind(KICK, key(KEY_L));
    l.bind(GRAB, key(KEY_H));
    l.bind(BLOCK, key(KEY_I));
    l.bind(DODGE, key(KEY_O));
    l.bind(LEAVE, key(KEY_BACKSPACE));
    return map.set_layout(0, l, shared);
}

// Выход P2 с клавиатуры — минус нампада, а не Backspace: двое за одной клавиатурой делили бы
// клавишу, и карта отказала бы в раскладке.
[[nodiscard]] bool set_p2(input::ActionMap& map, input::SharedInput* shared) {
    input::ActionLayout l;
    bind_pad(l);
    l.bind_axis(MOVE_X, key(KEY_RIGHT), key(KEY_LEFT), fix32{});
    l.bind_axis(MOVE_Z, key(KEY_DOWN), key(KEY_UP), fix32{});
    l.bind(JUMP, key(KEY_KP0));
    l.bind(PUNCH, key(KEY_KP1));
    l.bind(KICK, key(KEY_KP2));
    l.bind(BLOCK, key(KEY_KP3));
    l.bind(CROSS, key(KEY_KP4));
    l.bind(GRAB, key(KEY_KP5));
    l.bind(DODGE, key(KEY_KP6));
    l.bind(LEAVE, key(KEY_KP_SUBTRACT));
    return map.set_layout(1, l, shared);
}

fix32 unit(fix32 v) { return fix32::from_int(fix32{} < v ? 1 : v < fix32{} ? -1 : 0); }

uint16_t with(uint16_t buttons, bool on, uint16_t bit) {
    return on ? static_cast<uint16_t>(buttons | bit) : buttons;
}

} // namespace

bool set_layouts(input::ActionMap& map, std::string& why) {
    static_assert(framework::brawl::SEATS == 2);
    input::SharedInput shared;
    if (set_p1(map, &shared) && set_p2(map, &shared)) return true;
    why = framework::input::shared_input_text(shared);
    return false;
}

PlayerCommand command_of(const input::InputFrame& f) {
    PlayerCommand c;
    framework::brawl::BrawlInput& in = c.input;
    in.present = true;
    in.move.move_x = unit(f.axes[MOVE_X]);
    in.move.move_z = unit(f.axes[MOVE_Z]);
    in.buttons = with(in.buttons, f.action_pressed(JUMP), bb::JUMP);
    in.buttons = with(in.buttons, f.action_held(BLOCK), bb::BLOCK);
    in.buttons = with(in.buttons, f.action_pressed(DODGE), bb::DODGE);
    if (f.action_pressed(PUNCH)) c.attack = Attack::Punch;
    else if (f.action_pressed(CROSS)) c.attack = Attack::Cross;
    else if (f.action_pressed(KICK)) c.attack = Attack::Kick;
    else if (f.action_pressed(GRAB)) c.attack = Attack::Grab;
    return c;
}

} // namespace rumble
