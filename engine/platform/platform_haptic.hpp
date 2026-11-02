#pragma once

// Платформенный шов Phone; реализацию выбирает CMake. Обычно используется через haptic.hpp.
namespace haptic::phone {
bool init();
void pulse(float strength, int ms);
void cancel();
void shutdown();
} // namespace haptic::phone
