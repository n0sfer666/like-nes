#pragma once
#include <cstddef>
#include <cstdint>

#include "scene_level.hpp"

namespace rumble {

using Level = framework::scene::Level;

bool read_table(const Level& level, const char* table_name, const uint8_t*& data, size_t& size);

} // namespace rumble
