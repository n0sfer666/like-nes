#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Lock-free SPSC (один продюсер / один консюмер) — связь sim→mixer (команды) и
// worker→mixer (PCM). RT-safe: без локов/heap в audio-callback. acquire/release дают
// happens-before между записью данных и публикацией индекса.
namespace audio {

// Очередь POD-команд. Cap — степень двойки.
template <typename T, size_t Cap>
class SpscQueue {
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be a power of two");

public:
    bool push(const T& v) { // только продюсер
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (h - t == Cap) return false; // полна
        buf_[h & (Cap - 1)] = v;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& out) { // только консюмер
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (h == t) return false; // пуста
        out = buf_[t & (Cap - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    // Прочитать голову без изъятия (консюмер) — для sample-accurate дренажа по времени.
    bool peek(T& out) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (h == t) return false;
        out = buf_[t & (Cap - 1)];
        return true;
    }

private:
    T buf_[Cap];
    std::atomic<size_t> head_{0}; // индекс записи продюсера
    std::atomic<size_t> tail_{0}; // индекс чтения консюмера
};

// Кольцо int16-сэмплов для стрим-декода (worker пишет впереди playhead, mixer читает).
class SampleRing {
public:
    void init(uint32_t frames) {
        buf_.assign(frames, 0);
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }
    uint32_t capacity() const { return static_cast<uint32_t>(buf_.size()); }

    // Продюсер (decode-worker): сколько фреймов влезет.
    uint32_t writable() const {
        return capacity() - (head_.load(std::memory_order_relaxed) -
                             tail_.load(std::memory_order_acquire));
    }
    uint32_t push(const int16_t* src, uint32_t n) {
        if (buf_.empty()) return 0; // незаинициализированное кольцо → без % на ноль
        uint32_t h = head_.load(std::memory_order_relaxed);
        uint32_t can = writable();
        if (n > can) n = can;
        for (uint32_t i = 0; i < n; ++i) buf_[(h + i) % buf_.size()] = src[i];
        head_.store(h + n, std::memory_order_release);
        return n;
    }

    // Консюмер (mixer): доступно фреймов.
    uint32_t readable() const {
        return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_relaxed);
    }
    int16_t pop_one() { // RT-safe, вызывает только mixer
        if (buf_.empty()) return 0;
        uint32_t t = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == t) return 0; // underrun → тишина
        int16_t v = buf_[t % buf_.size()];
        tail_.store(t + 1, std::memory_order_release);
        return v;
    }

private:
    std::vector<int16_t> buf_;
    std::atomic<uint32_t> head_{0};
    std::atomic<uint32_t> tail_{0};
};

} // namespace audio
