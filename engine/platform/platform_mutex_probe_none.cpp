#include "platform_mutex_probe.hpp"

namespace platform::mutex_probe {
void begin() {}
uint64_t end() { return 0; }
bool supported() { return false; }
bool exercise_mutexes() { return false; }
} // namespace platform::mutex_probe
