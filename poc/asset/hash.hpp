#pragma once
#include <cstddef>
#include <cstdint>

// FNV-1a 64 — детерминированный content-hash (тот же паттерн, что sim golden-hash
// determinism_test.cpp). Байт-ориентированный → не зависит от endianness машины.
namespace asset {

constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
constexpr uint64_t FNV_PRIME = 1099511628211ull;

inline uint64_t fnv1a(const void* data, size_t len, uint64_t h = FNV_OFFSET) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= FNV_PRIME;
    }
    return h;
}

} // namespace asset
