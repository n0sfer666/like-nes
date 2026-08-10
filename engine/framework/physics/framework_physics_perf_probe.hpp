#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "contact.hpp"
#include "framework_physics_load.hpp"
#include "solver.hpp"
#include "world.hpp"

// Измерительная половина гейта 8 спеки #15: прогон сцены, снятие счётчиков и печать. Утверждения
// живут отдельно (`framework_physics_perf_test.cpp`) — у них разные причины меняться: замер правится
// вслед за формой счётчиков, утверждения — вслед за поведением шага.
//
// `framework_alloc_probe.hpp` этот заголовок НЕ включает, хотя и читает его счётчик: тот подменяет
// глобальные `operator new`/`delete` всей программе и по собственному контракту допустим ровно в
// одном TU. Заголовок с именем «замер» протаскивал бы подмену транзитивно каждому, кто захочет
// померить сцену. Поэтому включает его ВЫЗЫВАЮЩИЙ, до этого файла, и он же отвечает за «ровно один».
namespace framework::physics::perf {

using Clock = std::chrono::steady_clock;

// Полный перебор для n тел — потолок, выше которого широкая фаза не вправе подняться НИКОГДА, на
// любой сцене: она не может рассмотреть больше пар, чем есть пар.
inline uint64_t full_sweep(uint64_t n) { return n * (n - 1) / 2; }

// Порог доли и число тел для полного перебора. Доля — величина ИЗ ЗАМЕРА, а не из алгоритма; чем
// она отличается от структурных потолков, сказано у `Breach`.
struct Bounds {
    uint64_t sweep_bodies = 0;
    uint64_t broad_percent = 100;
};

// Нарушения потолков, отмеченные ПОКАДРОВО. Пик по каждому полю берётся независимо от остальных, и
// сравнение пиков между собой — утверждение слабее заявленного: кадр с 800 парами и 19000 проекций
// пробивает «точки × итерации» вдвое, но проходит сравнение с пиком пар из ДРУГОГО кадра. Ложных
// падений это не давало, зато молча пропускало настоящие, поэтому неравенства проверяются на
// счётчиках одного кадра, а пики остаются тем, чем они и были, — эталоном и строкой отчёта.
struct Breach {
    bool broad_sweep = false;
    bool broad_share = false;
    bool pairs_vs_broad = false;
    bool narrow_vs_pairs = false;
    bool velocity = false;
    bool position = false;
};

struct Peak {
    uint64_t broad_candidates = 0;
    uint64_t pairs = 0;
    uint64_t narrow_checks = 0;
    uint64_t velocity_projections = 0;
    uint64_t position_projections = 0;
    uint64_t active_bodies = 0;
    double worst_frame_us = 0.0;
    double total_us = 0.0;
    long allocs = 0;
    Breach breach;
};

inline void note_breach(const WorkCounters& c, const Bounds& b, Breach& out) {
    const uint64_t sweep = full_sweep(b.sweep_bodies);
    if (c.broad_candidates > sweep) out.broad_sweep = true;
    if (c.broad_candidates > sweep * b.broad_percent / 100) out.broad_share = true;
    if (c.pairs > c.broad_candidates) out.pairs_vs_broad = true;
    if (c.narrow_checks > c.pairs) out.narrow_vs_pairs = true;
    if (c.velocity_projections > c.pairs * MAX_MANIFOLD_POINTS * VELOCITY_ITERATIONS)
        out.velocity = true;
    if (c.position_projections > c.pairs * MAX_MANIFOLD_POINTS * POSITION_ITERATIONS)
        out.position = true;
}

// Пик, а не среднее. Кадр — единица бюджета: сцена, где один кадр из двухсот стоил в двадцать раз
// дороже остальных, укладывается в средний бюджет и роняет игру в тот самый кадр.
inline Peak measure(World& w, uint32_t warmup, uint32_t measured, const Bounds& bounds) {
    const fix32 dt = load::step_dt();
    for (uint32_t i = 0; i < warmup; ++i) w.step(dt);

    Peak p;
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (uint32_t i = 0; i < measured; ++i) {
        const Clock::time_point t0 = Clock::now();
        w.step(dt);
        const double us =
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count()) /
            1000.0;
        const WorkCounters& c = w.counters();
        note_breach(c, bounds, p.breach);
        if (c.broad_candidates > p.broad_candidates) p.broad_candidates = c.broad_candidates;
        if (c.pairs > p.pairs) p.pairs = c.pairs;
        if (c.narrow_checks > p.narrow_checks) p.narrow_checks = c.narrow_checks;
        if (c.velocity_projections > p.velocity_projections)
            p.velocity_projections = c.velocity_projections;
        if (c.position_projections > p.position_projections)
            p.position_projections = c.position_projections;
        if (c.active_bodies > p.active_bodies) p.active_bodies = c.active_bodies;
        p.total_us += us;
        if (us > p.worst_frame_us) p.worst_frame_us = us;
    }
    p.allocs = framework::probe::allocs;
    framework::probe::in_hot = false;
    return p;
}

inline void report(const char* name, const Peak& p, uint32_t measured) {
    std::printf("  %s: bodies=%llu pairs=%llu broad=%llu narrow=%llu vel=%llu pos=%llu\n", name,
                static_cast<unsigned long long>(p.active_bodies),
                static_cast<unsigned long long>(p.pairs),
                static_cast<unsigned long long>(p.broad_candidates),
                static_cast<unsigned long long>(p.narrow_checks),
                static_cast<unsigned long long>(p.velocity_projections),
                static_cast<unsigned long long>(p.position_projections));
    std::printf("  %s: worst=%.3f ms mean=%.3f ms allocs=%ld\n", name, p.worst_frame_us / 1000.0,
                p.total_us / static_cast<double>(measured) / 1000.0, p.allocs);
}

} // namespace framework::physics::perf
