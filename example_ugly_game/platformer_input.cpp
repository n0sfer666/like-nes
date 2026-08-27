#include "platformer_input.hpp"

namespace platformer {

bool resolve_binding(const framework::input::PresetTable& table, const char* preset, Binding& out) {
    const int index = table.find_preset(preset);
    if (index < 0) return false;
    const uint32_t p = static_cast<uint32_t>(index);
    out.jump = table.find_action(p, "jump");
    out.move_x = table.find_axis(p, "move_x");
    out.move_y = table.find_axis(p, "move_y");
    return out.valid();
}

ch::MoveInput read_input(const ::input::InputFrame& frame, const Binding& bind) {
    ch::MoveInput in;
    if (!bind.valid()) return in;
    in.move_x = frame.axes[bind.move_x];
    in.jump_held = frame.action_held(bind.jump);
    // Знак переворачивается ЗДЕСЬ, на границе ввода, а не в контроллере: второе место, где он
    // переворачивался бы, — второе место, где он однажды разъедется с первым.
    in.down_held = frame.axes[bind.move_y].raw < DOWN_EDGE.raw;
    return in;
}

} // namespace platformer
