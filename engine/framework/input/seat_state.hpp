#pragma once
#include <cstdint>

namespace framework::input {

// Joining и Resuming держат игрока ровно один тик: нажатие, занявшее устройство, разрешается в
// том же тике и становится уровнем, поэтому на следующем тике оно уже не фронт и не удар.
enum class SeatState : uint8_t { Free, Joining, Present, Lost, Resuming };

} // namespace framework::input
