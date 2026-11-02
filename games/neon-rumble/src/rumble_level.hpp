#pragma once
#include <cstddef>
#include <cstdint>

#include "rumble_stage.hpp"

namespace rumble {

using Level = Stage;

bool read_table(const asset::BundleView& bundle, const char* table_name, const uint8_t*& data, size_t& size);
bool read_table(const Level& level, const char* table_name, const uint8_t*& data, size_t& size);

} // namespace rumble
