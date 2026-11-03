#pragma once
#include <algorithm>
#include <cstdint>

#include "fixed.hpp"

namespace audio {
// RT-owned Q16 gain. Текущее значение — на границе следующего sample, advance идёт после mix.
// Интерполяция абсолютная, без накопления округлений; endpoint достигнут после N advance.
class MusicEnvelope {
public:
    constexpr int32_t value() const {
        if (!length_ || elapsed_ == length_) return to_;
        const int64_t delta = static_cast<int64_t>(to_) - from_;
        return static_cast<int32_t>(from_ + delta * elapsed_ / length_);
    }
    constexpr void reset(int32_t raw) {
        from_ = to_ = std::clamp(raw, int32_t{0}, fix32::ONE); length_ = elapsed_ = 0;
    }
    constexpr void ramp_to(int32_t raw, uint32_t frames) {
        from_ = value(); to_ = std::clamp(raw, int32_t{0}, fix32::ONE);
        length_ = frames; elapsed_ = 0;
    }
    constexpr void hold() { reset(value()); }
    constexpr void advance(uint32_t frames = 1) { elapsed_ += std::min(frames, length_ - elapsed_); }
    constexpr bool done() const { return elapsed_ == length_; }
private:
    int32_t from_ = 0, to_ = 0;
    uint32_t length_ = 0, elapsed_ = 0;
};
}
