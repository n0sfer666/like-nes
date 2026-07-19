#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bundle_view.hpp"
#include "byte_arena.hpp"
#include "platform_io.hpp"

// Рантайм-загрузчик (спека #5). Гибрид I/O: mmap-резидент zero-copy (Mmap) + async-стрим
// с декомпрессией в арену (Stream) на worker-потоке (НЕ sim-поток). Готовность ассета =
// ДЕТЕРМИНИРОВАННЫЙ gate: sim видит ready-set только в sync_point() тика; тайминг диска
// НЕ протекает в sim (инвариант #2/#3 спеки #1). Никаких per-frame heap в submit-пути.
namespace asset {

// ВНИМАНИЕ: Loaded.data валиден до следующего reload()/close() — reload ремапит бандл и
// ресетит арену, делая ранее выданные указатели висячими. Держатель обязан перезапросить get().
struct Loaded {
    const uint8_t* data = nullptr; // mmap (zero-copy) или арена (decompress)
    uint32_t size = 0;
    bool zero_copy = false;
};

class AssetManager {
public:
    // arena_capacity — резидентная арена под декомпрессированные Stream-payload'ы.
    // io_delay_us — инъекция задержки worker'а (гейт #3: замедленный I/O → тот же sim-hash).
    AssetManager() = default;
    ~AssetManager() { close(); } // join worker — иначе terminate при joinable std::thread

    bool open(const std::string& bundle_path, size_t arena_capacity, bool trusted,
              unsigned io_delay_us = 0);
    void close();

    const BundleView& view() const { return view_; }
    IoCaps caps() const { return MappedFile::caps(); }

    // request/release — refcount; pin — always-resident. Mmap-ассеты готовы сразу (zero-copy),
    // Stream — ставятся в очередь worker'у. PoC-ограничение: release при refcount→0 НЕ выселяет
    // из ready-set и не освобождает арену (резидент до reload/close). Budget-LRU-эвикция и
    // компакция арены — спроектированная точка расширения (ADR 0003), в walking-skeleton не реализована.
    void request(uint64_t guid);
    void release(uint64_t guid);
    void pin(uint64_t guid);

    // Hot-reload: свап бандла на новый (rebake) — guid стабилен, refcount'ы переживают,
    // запрошенные ассеты перезагружаются с НОВЫМ содержимым (спека #5 hot-reload roundtrip).
    bool reload(const std::string& new_bundle_path);

    // Публикация завершённых загрузок в видимый sim/render ready-set (детерм. gate тика).
    void sync_point();
    bool is_ready(uint64_t guid) const;
    Loaded get(uint64_t guid) const;

    // Диагностика (лог, не гейт): пик арены, число арен-аллокаций, завершённые загрузки.
    size_t arena_used() const { return arena_.used(); }
    uint64_t arena_allocations() const { return arena_.allocations(); }

private:
    struct Slot {
        const AssetEntry* entry = nullptr;
        int refcount = 0;
        bool pinned = false;
        std::atomic<bool> completed{false};
        std::atomic<bool> inflight{false}; // уже в очереди worker'а → без дубля-задачи
        Loaded loaded;
    };

    void submit_load(uint64_t guid); // под mu_: mmap→готов сразу, Stream→в очередь
    void start_worker();
    void stop_worker();
    void worker_loop();
    void do_load(Slot& s);

    MappedFile file_;
    BundleView view_;
    ByteArena arena_;
    bool trusted_ = true;
    unsigned io_delay_us_ = 0;

    mutable std::mutex mu_;
    std::unordered_map<uint64_t, Slot> slots_;
    std::unordered_map<uint64_t, Loaded> visible_; // published ready-set
    std::queue<uint64_t> jobs_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stop_ = false;
};

} // namespace asset
