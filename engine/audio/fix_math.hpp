#pragma once
#include <cstdint>

#include "../core/fixed.hpp"

// Детерминированная fix32-математика для микса (равная на x86/ARM): целочисл. sqrt
// для equal-power пана и distance-аттенюации. Никаких float в детерм.-пути.
namespace audio {

// Целочисленный sqrt (bit-by-bit) — без float, детерминирован cross-arch.
inline uint32_t isqrt64(uint64_t n) {
    uint64_t res = 0;
    uint64_t bit = 1ull << 62;
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<uint32_t>(res);
}

// sqrt(v) в Q16.16: result.raw = isqrt(v.raw << 16). Отрицательное → 0.
inline fix32 fix_sqrt(fix32 v) {
    if (v.raw <= 0) return fix32();
    return fix32::from_raw(static_cast<int32_t>(isqrt64(static_cast<uint64_t>(v.raw) << 16)));
}

inline fix32 fix_abs(fix32 v) { return v.raw < 0 ? fix32::from_raw(fix32::sat(-static_cast<int64_t>(v.raw))) : v; }

inline fix32 fix_clamp01(fix32 v) {
    if (v.raw < 0) return fix32();
    if (v.raw > fix32::ONE) return fix32::from_int(1);
    return v;
}

} // namespace audio
