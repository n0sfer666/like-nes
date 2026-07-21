#include "command.hpp"
#include "scene.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

// Гейт 7 (спека #7): отзывчивость на большой сцене (10k+). Выделение / property-grid / undo /
// виртуализация ≤ бюджет + БЕЗ per-frame heap в hot-UI. Счётчик C++-аллокаций (override operator
// new) доказывает zero-alloc в per-frame пути (ловит std::vector/string/function — доминирующий
// UI-риск; flecs try_get — table-lookup C-API без malloc/new). Масштаб-инвариантность (10k↔50k)
// доказывает O(1)/O(видимого), а не O(N).
using namespace ide;

namespace {

std::atomic<size_t> g_allocs{0};
bool g_count = false;
volatile uint64_t g_sink = 0;   // материализация hot-loop результатов (защита от DCE в Release)

// Timing-бюджеты валидны только в оптимизированной сборке без санитайзеров (ASan/UBSan/TSan дают
// ×2-3 оверхед → флейки). Под санитайзером проверяем только zero-alloc + корректность.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__) || \
    (defined(__has_feature) && (__has_feature(address_sanitizer) || __has_feature(thread_sanitizer)))
#define IDE_PERF_TIMING 0
#else
#define IDE_PERF_TIMING 1
#endif

using Clock = std::chrono::steady_clock;
double us_since(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t).count() / 1000.0;
}

int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("  FAIL: %s\n", w); ++failures; } }
void check_time(bool c, const char* w) { if (IDE_PERF_TIMING) check(c, w); }

void populate(Scene& s, uint32_t n, std::vector<uint64_t>& flat) {
    flat.clear();
    flat.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint64_t g = 1000 + i;
        auto e = s.create(g);
        e.set<Position>({fix32::from_int(static_cast<int32_t>(i % 256)),
                         fix32::from_int(static_cast<int32_t>(i / 256))});
        if (i % 2 == 0) e.set<Velocity>({fix32::from_raw(static_cast<int32_t>(i)), fix32()});
        flat.push_back(g);
    }
    std::sort(flat.begin(), flat.end());
}

// property-grid hot-путь: typed try_get → snprintf в фикс. буфер (zero-alloc). len клампится.
int grid_read(flecs::entity e, char* buf, size_t cap) {
    size_t len = 0;
    const Position* p = e.try_get<Position>();
    const Velocity* v = e.try_get<Velocity>();
    if (p && len < cap) {
        int w = std::snprintf(buf + len, cap - len, "pos %d,%d ", p->x.raw, p->y.raw);
        if (w > 0) len += static_cast<size_t>(w);
        if (len > cap) len = cap;
    }
    if (v && len < cap) {
        int w = std::snprintf(buf + len, cap - len, "vel %d,%d", v->x.raw, v->y.raw);
        if (w > 0) len += static_cast<size_t>(w);
        if (len > cap) len = cap;
    }
    return static_cast<int>(len);
}

} // namespace

void* operator new(size_t n) {
    if (g_count) g_allocs.fetch_add(1, std::memory_order_relaxed);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

int main() {
    const uint32_t N = 10000;
    Scene s;
    std::vector<uint64_t> flat;
    populate(s, N, flat);

    std::vector<uint64_t> selection;
    selection.reserve(N);
    char grid[256];

    // --- Bench A: выделение (скан сцены → преаллок. буфер) ≤ бюджет + zero C++-alloc ---
    double sel_us = 0;
    {
        g_allocs.store(0); g_count = true;
        auto t = Clock::now();
        selection.clear();
        for (const auto& [guid, e] : s.entities())
            if (e.try_get<Velocity>()) selection.push_back(guid);
        sel_us = us_since(t);
        g_count = false;
        check(g_allocs.load() == 0, "selection scan: zero per-frame C++ heap alloc");
        check(selection.size() == N / 2, "selection collected expected count");
        check_time(sel_us < 1500.0, "selection over 10k <= 1.5ms budget");
    }

    // --- Bench B: property-grid чтение выбранной сущности, per-frame (O(1) в размере сцены) ---
    double grid_us = 0;
    {
        flecs::entity sel = s.get(selection[0]);
        g_allocs.store(0); g_count = true;
        auto t = Clock::now();
        uint64_t acc = 0;
        for (int frame = 0; frame < 1000; ++frame)
            acc += static_cast<uint64_t>(grid_read(sel, grid, sizeof(grid))) + static_cast<unsigned char>(grid[0]);
        grid_us = us_since(t) / 1000.0;
        g_count = false;
        g_sink += acc;
        check(g_allocs.load() == 0, "property-grid read: zero per-frame C++ heap alloc");
        check_time(grid_us < 5.0, "property-grid read <= 5us/frame budget");
    }

    // --- Bench C: виртуализация (видимое окно K из N) — O(K), zero-alloc. sink материализуется ---
    double virt_us = 0;
    {
        const size_t K = 64, off = 5000;
        g_allocs.store(0); g_count = true;
        auto t = Clock::now();
        uint64_t sink = 0;
        for (int frame = 0; frame < 1000; ++frame)
            for (size_t i = off; i < off + K && i < flat.size(); ++i) sink += flat[i];
        virt_us = us_since(t) / 1000.0;
        g_count = false;
        g_sink += sink;   // защита от DCE: результат наблюдаем
        check(g_allocs.load() == 0, "virtualized list window: zero per-frame C++ heap alloc");
        check_time(virt_us < 2.0, "virtualized window (K=64) <= 2us/frame budget");
    }

    // --- Bench D: undo-операция на 10k-сцене — латентность (per-action, не per-frame) ---
    double undo_us = 0;
    {
        CommandBus bus(s);
        auto t = Clock::now();
        const int ops = 1000;
        for (int i = 0; i < ops; ++i) {
            bus.set_component<Position>(selection[i % selection.size()],
                                        {fix32::from_int(i), fix32()});
            bus.undo();
        }
        undo_us = us_since(t) / ops;
        check_time(undo_us < 15.0, "undo op on 10k scene <= 15us budget");
    }

    // --- Масштаб-инвариантность: 50k сцена — те же операции; мультипликативный порог ловит O(N) ---
    {
        Scene s2;
        std::vector<uint64_t> flat2;
        populate(s2, 50000, flat2);
        flecs::entity sel2 = s2.get(1000);   // i=0 → чётный → есть Position и Velocity (как sel)

        auto t = Clock::now();
        uint64_t acc = 0;
        for (int frame = 0; frame < 1000; ++frame)
            acc += static_cast<uint64_t>(grid_read(sel2, grid, sizeof(grid))) + static_cast<unsigned char>(grid[0]);
        double grid50 = us_since(t) / 1000.0;
        g_sink += acc;
        check_time(grid50 < grid_us * 4.0 + 0.5, "property-grid O(1) in scene size (50k <= 4x 10k)");

        CommandBus bus2(s2);
        t = Clock::now();
        for (int i = 0; i < 1000; ++i) {
            bus2.set_component<Position>(1000 + static_cast<uint64_t>((i % 40000) * 2), {fix32::from_int(i), fix32()});
            bus2.undo();
        }
        double undo50 = us_since(t) / 1000.0;
        check_time(undo50 < undo_us * 4.0 + 1.0, "undo O(1) in scene size (50k <= 4x 10k)");
    }

    std::printf("perf: selection=%.1fus grid=%.3fus/f virt=%.3fus/f undo=%.2fus/op (timing-asserts=%d) sink=%llu\n",
                sel_us, grid_us, virt_us, undo_us, IDE_PERF_TIMING,
                static_cast<unsigned long long>(g_sink));
    bool pass = (failures == 0);
    std::printf("ide-perf: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
