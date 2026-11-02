#include "platform_haptic.hpp"

namespace haptic::phone {
bool init() { return false; }
void pulse(float, int) {}
void cancel() {}
void shutdown() {}
} // namespace haptic::phone
