#pragma once
#include <atomic>
#include <cstdint>

// Seqlock: single-writer / multi-reader консистентный снапшот без блокировок. Writer делает seq
// нечётным на время записи; reader копирует данные и проверяет, что seq не изменился и чётный.
// Data-plane намеренно не-атомарен (zero-copy POD); корректность = функциональный retry, не
// TSan (в гейтах writer/reader — в РАЗНЫХ процессах через mmap → внутрипроцессной гонки нет,
// TSan её не видит; same-process self-test валидирует консистентность по gen-полю).
namespace ide::ipc {

inline void seq_write_begin(std::atomic<uint64_t>& seq) {
    uint64_t s = seq.load(std::memory_order_relaxed);
    seq.store(s + 1, std::memory_order_relaxed);              // нечёт
    std::atomic_thread_fence(std::memory_order_release);
}

inline void seq_write_end(std::atomic<uint64_t>& seq) {
    std::atomic_thread_fence(std::memory_order_release);
    uint64_t s = seq.load(std::memory_order_relaxed);
    seq.store(s + 1, std::memory_order_relaxed);              // чёт
}

// Возвращает true, если copy() захватил консистентный снапшот. Иначе — повторить.
template <typename CopyFn>
inline bool seq_try_read(const std::atomic<uint64_t>& seq, CopyFn&& copy) {
    uint64_t s1 = seq.load(std::memory_order_acquire);
    if (s1 & 1u) return false;                               // writer активен
    copy();
    std::atomic_thread_fence(std::memory_order_acquire);
    uint64_t s2 = seq.load(std::memory_order_relaxed);
    return s1 == s2;
}

// Retry до успеха (или до max_spins → false, чтобы reader не завис при мёртвом writer'е).
template <typename CopyFn>
inline bool seq_read(const std::atomic<uint64_t>& seq, CopyFn&& copy, int max_spins = 1000000) {
    for (int i = 0; i < max_spins; ++i) {
        if (seq_try_read(seq, copy)) return true;
    }
    return false;
}

} // namespace ide::ipc
