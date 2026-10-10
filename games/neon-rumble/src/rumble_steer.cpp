#include "rumble_steer.hpp"

namespace rumble {

int32_t toward(fix32 from, fix32 to, int32_t arrive) {
    if (framework::ai::axis_gap(from, to) <= fix32::from_int(arrive).raw) return 0;
    return from < to ? 1 : -1;
}

} // namespace rumble
