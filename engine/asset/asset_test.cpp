#include <chrono>
#include <thread>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "hash.hpp"

// Headless-гейт #2 (спека #5): zero-copy mmap-резидент + async-стрим→декомпрессия в арену,
// БЕЗ per-frame heap в submit-пути. Гоняется под ASan/UBSan (CI). Не требует GPU.

using namespace asset;

namespace {

uint64_t guid_of(const char* n) { return fnv1a(n, std::strlen(n)); }

// Ждём готовности всех запрошенных ассетов через sync_point'ы (детерм. gate тика).
void pump_until_ready(AssetManager& am, const std::vector<uint64_t>& guids, int max_frames) {
    for (int f = 0; f < max_frames; ++f) {
        am.sync_point();
        bool all = true;
        for (uint64_t g : guids) all = all && am.is_ready(g);
        if (all) return;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

int fail(const char* msg) {
    std::fprintf(stderr, "[asset_test] FAIL: %s\n", msg);
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return fail("usage: asset_test <bundle> [--selftest]");
    const std::string bundle = argv[1];
    const unsigned delay = (argc >= 3 && std::strcmp(argv[2], "--slow") == 0) ? 3000 : 0;

    AssetManager am;
    // validated-режим (bounds-check offset'ов) — строже, чем trusted.
    if (!am.open(bundle, 4u * 1024 * 1024, /*trusted=*/false, delay))
        return fail("open/validate bundle");

    const uint64_t shader_vs = guid_of("sprite.vs");
    const uint64_t albedo = guid_of("hero_albedo");
    const uint64_t bulk = guid_of("scene_bulk");
    std::vector<uint64_t> all = {shader_vs, albedo, bulk, guid_of("sprite.fs"),
                                 guid_of("hero_normal")};

    for (uint64_t g : all) am.request(g);
    am.request(bulk); am.request(albedo); // двойной запрос in-flight — дедуп (high-фикс)
    pump_until_ready(am, all, 500);

    for (uint64_t g : all)
        if (!am.is_ready(g)) return fail("asset not ready after pump");

    // In-flight дедуп (high-фикс): 3 Stream-ассета (bulk+albedo+normal) → ровно 3 арен-аллок,
    // несмотря на двойной request (без дедупа было бы 5). Shader'ы zero-copy — 0 аллок.
    if (am.arena_allocations() != 3) return fail("in-flight dedup: double request re-loaded");

    // (1) Шейдер — zero-copy: указатель ВНУТРИ mmap-региона, content_hash сходится.
    Loaded vs = am.get(shader_vs);
    const AssetEntry* vse = am.view().find(shader_vs);
    if (!vs.zero_copy || vs.data != am.view().payload(*vse)) return fail("shader not zero-copy");
    if (fnv1a(vs.data, vs.size) != vse->content_hash) return fail("shader content_hash");

    // (2) Bulk — zstd декомпрессия в арену корректна (сверка детерм. паттерна).
    Loaded b = am.get(bulk);
    const AssetEntry* be = am.view().find(bulk);
    if (b.zero_copy || b.size != be->uncompressed_size) return fail("bulk size/zero-copy");
    for (uint32_t i = 0; i < b.size; ++i)
        if (b.data[i] != static_cast<uint8_t>((i * 2654435761u) >> 24)) return fail("bulk bytes");

    // (3) Текстура — staged в арену (Phase 3 транскодит BC7).
    Loaded t = am.get(albedo);
    if (t.zero_copy || t.size == 0) return fail("texture staging");

    // (4) Гейт #2: steady-state submit-петля НЕ растит арену (нет per-frame heap-аллокаций).
    uint64_t allocs_before = am.arena_allocations();
    for (int frame = 0; frame < 120; ++frame) {
        am.sync_point();
        volatile const uint8_t* sink = am.get(albedo).data; // «сабмит» без аллокаций
        (void)sink;
    }
    if (am.arena_allocations() != allocs_before) return fail("per-frame arena growth");

    std::printf("[asset_test] PASS zero-copy=shader stream=bulk+tex arena_used=%zu allocs=%llu "
                "delay_us=%u\n",
                am.arena_used(), (unsigned long long)am.arena_allocations(), delay);
    am.close();
    (void)argc;
    return 0;
}
