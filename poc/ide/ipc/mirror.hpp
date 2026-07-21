#pragma once
#include <atomic>
#include <cstdint>

// Read-only IPC-зеркало состояния game-процесса (спека #7, гейт 3). POD-layout → zero-copy
// (без маршалинга). Заголовок несёт schema_hash/layout_version: hot-reload .so меняет layout →
// редактор отвергает несовместимый снапшот, а не читает мусор. seqlock (single-writer game /
// multi-reader editor) даёт консистентный снапшот без блокировок.
namespace ide::ipc {

constexpr uint32_t MIRROR_MAGIC = 0x4C4E4D52;      // 'LNMR'
constexpr uint32_t MIRROR_LAYOUT_VERSION = 1;
constexpr uint32_t MIRROR_CAPACITY = 16384;        // >= 10k гейта

struct MirrorEntity {
    uint64_t guid;
    int32_t px, py, vx, vy;   // fix32 raw (целочисл. → cross-arch)
    uint32_t gen;             // поколение снапшота (для детекта torn-read в self-test)
};

struct MirrorHeader {
    uint32_t magic;
    uint32_t layout_version;
    uint64_t schema_hash;
    std::atomic<uint64_t> seq; // seqlock: нечёт = запись идёт
    uint32_t capacity;
    uint32_t count;
};

struct MirrorBuffer {
    MirrorHeader header;
    MirrorEntity entities[MIRROR_CAPACITY];
};

// seq живёт в shmem двух процессов → атомик обязан быть lock-free (address-keyed lock сломал бы
// синхронизацию через границу процессов: два разных lock'а на два маппинга).
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "mirror seq must be lock-free for cross-process shared memory");

constexpr uint64_t fnv_mix(uint64_t h, uint64_t v) {
    for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 0xFFu; h *= 1099511628211ull; }
    return h;
}

// Хеш layout'а — reader сверяет со своим; несовпадение → reject (layout-drift).
constexpr uint64_t mirror_schema_hash() {
    uint64_t h = 1469598103934665603ull;
    h = fnv_mix(h, sizeof(MirrorEntity));
    h = fnv_mix(h, sizeof(MirrorHeader));
    h = fnv_mix(h, MIRROR_LAYOUT_VERSION);
    h = fnv_mix(h, MIRROR_CAPACITY);
    return h;
}

} // namespace ide::ipc
