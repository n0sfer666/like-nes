#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fuzz {
std::vector<uint8_t> seed_fighter();
bool read_fighter(const uint8_t* data, std::size_t size);
} // namespace fuzz
