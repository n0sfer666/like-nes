#include "hitstop.hpp"

namespace framework::hits {

bool hitstop_tick(uint16_t& ticks) {
    if (ticks == 0) return false;
    --ticks;
    return true;
}

} // namespace framework::hits
