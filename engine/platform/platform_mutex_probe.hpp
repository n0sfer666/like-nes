#pragma once

#include <cstdint>

namespace platform::mutex_probe {
void begin();
uint64_t end();
bool supported();
bool exercise_mutexes();
} // namespace platform::mutex_probe
