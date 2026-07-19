#include "asset_manager.hpp"

#include <unistd.h>

#include <cstring>
#include <zstd.h>

namespace asset {

bool AssetManager::open(const std::string& bundle_path, size_t arena_capacity, bool trusted,
                        unsigned io_delay_us) {
    trusted_ = trusted;
    io_delay_us_ = io_delay_us;
    if (!file_.open(bundle_path)) return false;
    if (!view_.open(file_.data(), file_.size(), trusted)) return false;
    arena_.init(arena_capacity);
    start_worker();
    return true;
}

void AssetManager::start_worker() {
    stop_ = false;
    worker_ = std::thread(&AssetManager::worker_loop, this);
}

void AssetManager::stop_worker() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void AssetManager::close() {
    stop_worker();
    slots_.clear();
    visible_.clear();
    view_ = BundleView{};
    file_.close();
}

void AssetManager::submit_load(uint64_t guid) {
    Slot& s = slots_[guid];
    if (!s.entry || s.completed.load()) return;
    if (s.entry->residency == static_cast<uint32_t>(Residency::Mmap)) {
        // zero-copy: указатель прямо в mmap-регион, готов немедленно.
        s.loaded = Loaded{view_.payload(*s.entry), s.entry->payload_size, true};
        s.completed.store(true);
        return;
    }
    if (s.inflight.exchange(true)) return; // уже в очереди — без дубля (иначе двойной do_load)
    jobs_.push(guid);
    cv_.notify_one();
}

void AssetManager::request(uint64_t guid) {
    std::lock_guard<std::mutex> lk(mu_);
    Slot& s = slots_[guid];
    if (!s.entry) s.entry = view_.find(guid);
    if (!s.entry) return; // отсутствующий ассет → placeholder на стороне рендера
    ++s.refcount;
    submit_load(guid);
}

bool AssetManager::reload(const std::string& new_bundle_path) {
    stop_worker(); // worker стоит → старый mmap на worker'е больше не читается

    // ТРАНЗАКЦИОННО: открыть+валидировать НОВЫЙ бандл во временные объекты. При провале —
    // старое состояние (file_/view_/visible_/slots) НЕ трогаем → get() остаётся валидным
    // (никакого munmap до успеха → нет UAF на битом rebake).
    MappedFile newf;
    if (!newf.open(new_bundle_path)) { start_worker(); return false; }
    BundleView newv;
    if (!newv.open(newf.data(), newf.size(), trusted_)) { start_worker(); return false; }

    std::vector<uint64_t> req;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [g, s] : slots_)
            if (s.refcount > 0) req.push_back(g);
        std::queue<uint64_t> empty;
        std::swap(jobs_, empty);
        file_ = std::move(newf); // munmap старого — ТОЛЬКО теперь, после валидации нового
        view_ = newv;            // base_ = тот же mmap-адрес (move не ремапит) → валиден
        arena_.reset();
        visible_.clear();
        for (auto& [g, s] : slots_) {
            s.entry = view_.find(g); // guid стабилен → новый entry того же ассета
            s.completed.store(false);
            s.inflight.store(false);
            s.loaded = Loaded{};
        }
    }
    start_worker();
    std::lock_guard<std::mutex> lk(mu_);
    for (uint64_t g : req) submit_load(g); // перезагрузка с новым содержимым
    return true;
}

void AssetManager::release(uint64_t guid) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = slots_.find(guid);
    if (it != slots_.end() && it->second.refcount > 0 && !it->second.pinned) --it->second.refcount;
}

void AssetManager::pin(uint64_t guid) {
    request(guid);
    std::lock_guard<std::mutex> lk(mu_);
    slots_[guid].pinned = true;
}

void AssetManager::worker_loop() {
    for (;;) {
        uint64_t guid;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return stop_ || !jobs_.empty(); });
            if (stop_ && jobs_.empty()) return;
            guid = jobs_.front();
            jobs_.pop();
        }
        // Инъекция задержки I/O ВНЕ sim-потока (гейт #3).
        if (io_delay_us_) usleep(io_delay_us_);
        Slot* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = slots_.find(guid);
            if (it != slots_.end()) s = &it->second;
        }
        if (s) do_load(*s);
    }
}

void AssetManager::do_load(Slot& s) {
    const AssetEntry& e = *s.entry;
    const uint8_t* src = view_.payload(e); // mmap-регион (чтение зоны стрима)
    if (e.codec == static_cast<uint32_t>(Codec::Zstd)) {
        uint8_t* dst = arena_.alloc(e.uncompressed_size);
        if (!dst) return;
        size_t n = ZSTD_decompress(dst, e.uncompressed_size, src, e.payload_size);
        if (ZSTD_isError(n) || n != e.uncompressed_size) return;
        s.loaded = Loaded{dst, e.uncompressed_size, false};
    } else {
        // Ktx2/прочее: staging копия в арену (транскод BC7 — Phase 3, читает отсюда).
        uint8_t* dst = arena_.alloc(e.payload_size);
        if (!dst) return;
        std::memcpy(dst, src, e.payload_size);
        s.loaded = Loaded{dst, e.payload_size, false};
    }
    s.completed.store(true); // release: loaded-запись выше happens-before видимого completed
    s.inflight.store(false);
}

void AssetManager::sync_point() {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [guid, s] : slots_)
        if (s.completed.load() && visible_.find(guid) == visible_.end())
            visible_[guid] = s.loaded;
}

bool AssetManager::is_ready(uint64_t guid) const {
    std::lock_guard<std::mutex> lk(mu_);
    return visible_.find(guid) != visible_.end();
}

Loaded AssetManager::get(uint64_t guid) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = visible_.find(guid);
    return it != visible_.end() ? it->second : Loaded{};
}

} // namespace asset
