#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// CPU-арена для транзитных буферов декомпрессии (инвариант #5 спеки #1: в load/submit-пути
// НЕТ per-frame heap). Ёмкость резервируется один раз; reset() переиспользует без free.
// alloc() возвращает nullptr при переполнении (жёсткий бюджет, без скрытого роста).
// alloc() зовёт единственный worker; cursor_/allocations_ atomic — диагностику безопасно
// читать из main-потока (relaxed: значения независимы, порядок не важен).
namespace asset {

class ByteArena {
public:
    void init(size_t capacity) {
        buf_.assign(capacity, 0);
        cursor_.store(0, std::memory_order_relaxed);
        allocations_.store(0, std::memory_order_relaxed);
    }
    void reset() { cursor_.store(0, std::memory_order_relaxed); }

    uint8_t* alloc(size_t n, size_t align = 16) {
        size_t cur = cursor_.load(std::memory_order_relaxed);
        size_t base = (cur + align - 1) & ~(align - 1);
        if (base + n > buf_.size()) return nullptr;
        cursor_.store(base + n, std::memory_order_relaxed);
        allocations_.fetch_add(1, std::memory_order_relaxed);
        return buf_.data() + base;
    }

    size_t used() const { return cursor_.load(std::memory_order_relaxed); }
    size_t capacity() const { return buf_.size(); }
    uint64_t allocations() const { return allocations_.load(std::memory_order_relaxed); }

private:
    std::vector<uint8_t> buf_;
    std::atomic<size_t> cursor_{0};
    std::atomic<uint64_t> allocations_{0};
};

} // namespace asset
