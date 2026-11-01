#pragma once
#include <cstdint>

namespace framework::hits {

// Последний тик срока тоже заморожен: решение принимается до декремента.
bool hitstop_tick(uint16_t& ticks);

} // namespace framework::hits
