#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "controller.hpp"
#include "framework_character_perf_level.hpp"
#include "trajectory.hpp"

// Измерительная половина гейта 7 спеки #16: прогон маршрута, снятие счётчиков и печать. Утверждения
// живут отдельно (`framework_character_perf_test.cpp`) — граница ровно та же, что у гейта 8 физики:
// замер правится вслед за формой счётчиков, утверждения — вслед за поведением тика.
//
// `framework_alloc_probe.hpp` этот заголовок НЕ включает, хотя и читает его счётчик: тот подменяет
// глобальные `operator new`/`delete` всей программе и по собственному контракту допустим ровно в
// одном TU. Включает его ВЫЗЫВАЮЩИЙ, до этого файла, и он же отвечает за «ровно один».
namespace framework::character::perf {

using Clock = std::chrono::steady_clock;

// Итог прогона. Работа снимается ТИКАМИ, а не суммой за прогон: единица бюджета — кадр, и уровень,
// где один тик из тысячи стоил в двадцать раз дороже прочих, укладывается в средний бюджет и роняет
// игру ровно в тот тик.
struct Run {
    uint64_t queries = 0;
    uint64_t scanned = 0;
    uint64_t worst_tick_queries = 0;
    uint64_t worst_tick_scanned = 0;
    uint64_t hash = 0;

    // Позитивные контроли маршрута: прогон, ни разу не тронувший воздух, склон и потолок, отвечает
    // про куда более узкий путь, чем «тик на уровне целевого размера».
    uint64_t ground_ticks = 0;
    uint64_t air_ticks = 0;
    uint64_t ceiling_ticks = 0;
    uint64_t slope_ticks = 0;
    int32_t min_col = 0;
    int32_t max_col = 0;

    double total_us = 0.0;
    double worst_tick_us = 0.0;
    long allocs = 0;
};

// Стоит ли персонаж на НАКЛОННОЙ грани. Мерится высотой подошв: у всякой плоской грани узора верх
// кратен тайлу, значит подошвы стоят на `16k - SKIN`, а между двумя такими уровнями персонаж
// оказывается только на гипотенузе. Проверять флаг тайла под ногами было бы слабее — тайл под
// центром корпуса на входе в склон ещё плоский.
inline bool feet_on_slope(fix32 feet) {
    for (uint32_t row = FLOOR_ROW - 2; row < FLOOR_ROW; ++row) {
        const fix32 lo = TILE_SIZE * fix32::from_int(static_cast<int32_t>(row));
        const fix32 hi = lo + TILE_SIZE;
        if (lo + fix32::from_int(1) < feet && feet < hi - fix32::from_int(1)) return true;
    }
    return false;
}

inline Run measure(const Scene& sc, uint32_t warmup, uint32_t measured) {
    const CollisionScene s = sc.view();
    const CharacterHull hull = make_hull();
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    Character c = start_character();

    // Прогрев идёт ДО счётчиков по той же причине, что и в гейте 8 физики: кучу и кеш инструкций
    // трогает не тик, а ленивая инициализация под ним, а обвинён был бы тик.
    for (uint32_t t = 0; t < warmup; ++t) step(s, hull, p, d, input_at(t), tick_dt(), c);

    Run r;
    r.min_col = r.max_col = static_cast<int32_t>((c.position.x / TILE_SIZE).to_int());
    TrajectoryHash h;
    sc.grid.reset_counters();
    uint64_t prev_queries = 0;
    uint64_t prev_scanned = 0;
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (uint32_t t = 0; t < measured; ++t) {
        const Clock::time_point t0 = Clock::now();
        step(s, hull, p, d, input_at(warmup + t), tick_dt(), c);
        const double us =
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count()) /
            1000.0;
        const tilemap::TileCounters& tc = sc.grid.counters();
        const uint64_t dq = tc.queries - prev_queries;
        const uint64_t ds = tc.scanned_total - prev_scanned;
        prev_queries = tc.queries;
        prev_scanned = tc.scanned_total;
        if (dq > r.worst_tick_queries) r.worst_tick_queries = dq;
        if (ds > r.worst_tick_scanned) r.worst_tick_scanned = ds;
        r.total_us += us;
        if (us > r.worst_tick_us) r.worst_tick_us = us;

        h.feed(c);
        if (c.on_ground) ++r.ground_ticks; else ++r.air_ticks;
        if (c.hit_ceiling) ++r.ceiling_ticks;
        if (c.on_ground && feet_on_slope(c.position.y + HULL_HALF_H)) ++r.slope_ticks;
        const int32_t col = static_cast<int32_t>((c.position.x / TILE_SIZE).to_int());
        if (col < r.min_col) r.min_col = col;
        if (col > r.max_col) r.max_col = col;
    }
    r.allocs = framework::probe::allocs;
    framework::probe::in_hot = false;
    r.queries = sc.grid.counters().queries;
    r.scanned = sc.grid.counters().scanned_total;
    r.hash = h.value;
    return r;
}

inline void report(const char* name, const Scene& sc, const Run& r, uint32_t measured) {
    std::printf("  %s: map=%ux%u (%llu tiles) queries=%llu scanned=%llu\n", name, sc.grid.width(),
                sc.grid.height(),
                static_cast<unsigned long long>(sc.grid.width()) * sc.grid.height(),
                static_cast<unsigned long long>(r.queries),
                static_cast<unsigned long long>(r.scanned));
    std::printf("  %s: worst tick = %llu queries / %llu tiles, hash=%016llx\n", name,
                static_cast<unsigned long long>(r.worst_tick_queries),
                static_cast<unsigned long long>(r.worst_tick_scanned),
                static_cast<unsigned long long>(r.hash));
    std::printf("  %s: worst=%.4f ms mean=%.4f ms allocs=%ld cols=[%d, %d]\n", name,
                r.worst_tick_us / 1000.0, r.total_us / static_cast<double>(measured) / 1000.0,
                r.allocs, r.min_col, r.max_col);
    std::printf("  %s: ground=%llu air=%llu ceiling=%llu slope=%llu\n", name,
                static_cast<unsigned long long>(r.ground_ticks),
                static_cast<unsigned long long>(r.air_ticks),
                static_cast<unsigned long long>(r.ceiling_ticks),
                static_cast<unsigned long long>(r.slope_ticks));
}

} // namespace framework::character::perf
