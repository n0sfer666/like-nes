#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

// Lock-free SPSC (один продюсер / один консюмер) — связь input-поток → sim.
// input-поток (async-опрос ОС) = продюсер сырых событий; sim-поток = консюмер @tick.
// acquire/release дают happens-before между записью данных и публикацией индекса.
// Зеркало audio::SpscQueue (вертикаль самодостаточна — как asset/audio).
namespace input {

template <typename T, size_t Cap>
class SpscQueue {
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be a power of two");

public:
    bool push(const T& v) { // только продюсер (input-поток)
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (h - t == Cap) return false; // полна → событие отброшено (лог на стороне вызова)
        buf_[h & (Cap - 1)] = v;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& out) { // только консюмер (sim @tick)
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (h == t) return false; // пуста
        out = buf_[t & (Cap - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

private:
    T buf_[Cap];
    std::atomic<size_t> head_{0}; // индекс записи продюсера
    std::atomic<size_t> tail_{0}; // индекс чтения консюмера
};

} // namespace input
