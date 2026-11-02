#pragma once
#include <cstdint>
#include <span>

// Внутренний шов checksum для format и reseal fuzz-цели; в SDK не поставляется.
namespace save::detail {
void reseal_record(std::span<uint8_t> encoded);
} // namespace save::detail
