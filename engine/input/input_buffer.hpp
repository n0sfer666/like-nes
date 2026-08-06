#pragma once
#include <cstdint>
#include "input_types.hpp"

// Кольцо последних N InputFrame: leniency-запросы («была ли Action нажата за последние n
// тиков») для прыжок-буфера и т.п. Combo/gesture-recognizer — точка расширения над буфером.
namespace input {

template <int N>
class InputBuffer {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");

public:
    void push(const InputFrame& f) { buf_[head_ & (N - 1)] = f; ++head_; }

    // pressed за последние n тиков (включая текущий). n<=N.
    bool pressed_within(int action, int n) const {
        if (n > N) n = N;
        uint64_t count = head_ < static_cast<uint64_t>(n) ? head_ : static_cast<uint64_t>(n);
        for (uint64_t i = 0; i < count; ++i)
            if (buf_[(head_ - 1 - i) & (N - 1)].action_pressed(action)) return true;
        return false;
    }

    const InputFrame& last() const { return buf_[(head_ - 1) & (N - 1)]; }
    uint64_t count() const { return head_; }

private:
    InputFrame buf_[N];
    uint64_t head_ = 0;
};

} // namespace input
