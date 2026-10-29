#include "action_buffer.hpp"

namespace framework::input {

void clear_action(ActionBuffer& buf) { buf = ActionBuffer{}; }

void buffer_action(ActionBuffer& buf, uint16_t action, uint16_t ticks) {
    buf = ticks == 0 ? ActionBuffer{} : ActionBuffer{action, ticks};
}

} // namespace framework::input
