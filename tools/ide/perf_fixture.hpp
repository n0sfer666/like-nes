#pragma once
#include "platform_noinline.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

// Харнесс гейта 7 (спека #7): счёт провалов, часы, повтор замера и счётчик C++-аллокаций. Вынесен
// из `perf_test.cpp` потому, что тот упёрся в жёсткий потолок длины, и починить в нём вакуумный
// контроль нового замера стало уже нечем (решение владельца по ревью аудита #21, A·2·8).
//
// Заголовок включается РОВНО ОДНИМ TU, и это условие корректности, а не осторожность на будущее:
// замена глобального `operator new` по стандарту не может быть `inline` (иначе она не замена), то
// есть второе включение даст второе определение и отказ линковщика. Держать инвариант обязан тот,
// кто заведёт второго потребителя: ему нужен свой .cpp с этими операторами, а не второй #include.
namespace ide::perf {

// Timing-бюджеты валидны только в оптимизированной сборке без санитайзеров (ASan/UBSan/TSan дают
// ×2-3 оверхед → флейки). Под санитайзером проверяем только zero-alloc + корректность.
// __has_feature нельзя звать в одном #if с defined() — GCC токенизирует всю строку и
// падает на __has_feature(...) (missing binary operator). Вкладываем в #ifdef.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define IDE_PERF_TIMING 0
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#    define IDE_PERF_TIMING 0
#  else
#    define IDE_PERF_TIMING 1
#  endif
#else
#define IDE_PERF_TIMING 1
#endif

inline std::atomic<size_t> g_allocs{0};
inline bool g_count = false;

inline int failures = 0;
inline void check(bool c, const char* w) { if (!c) { std::printf("  FAIL: %s\n", w); ++failures; } }

// Под санитайзером бюджет времени выключен ЦЕЛИКОМ — значит контроль самого замера обязан идти
// через `check`, а не через `check_time`: утверждение о времени там не выполняется вовсе.
inline void check_time(bool c, const char* w) { if (IDE_PERF_TIMING) check(c, w); }

using Clock = std::chrono::steady_clock;
inline double us_since(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t).count() / 1000.0;
}

// Раннер — общая виртуалка, её планировщик отбирает слайс прямо посреди замера. Одиночный прогон
// отличить кражу от регрессии не может в принципе, повтор — может: preemption бьёт по ОДНОМУ
// прогону, регрессия по всем трём. Минимум измеряет способность машины, а не её загруженность в
// конкретную секунду. Бюджеты при этом НЕ трогаются: прогон 31255611521 упал на macOS с
// undo=21.52us/оп при бюджете 15, тогда как здоровые прогоны той же ревизии дают 0.35 (macOS),
// 0.31 (Linux), 0.43 (Windows) — выброс в шестьдесят раз, а не «macOS медленнее». Поднять бюджет
// под такое значило бы ослепнуть к настоящему O(N)-регрессу, видимому сейчас с 40-кратным запасом.
template <class Body>
double best_of_three(Body&& body) {
    double best = body();
    for (int rep = 1; rep < 3; ++rep) {
        const double us = body();
        if (us < best) best = us;
    }
    return best;
}

} // namespace ide::perf

// Запрет инлайна обязателен, а не косметика — почему, см. platform_noinline.hpp.

PLATFORM_NOINLINE void* operator new(size_t n) {
    if (ide::perf::g_count) ide::perf::g_allocs.fetch_add(1, std::memory_order_relaxed);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
PLATFORM_NOINLINE void operator delete(void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete(void* p, size_t) noexcept { std::free(p); }
PLATFORM_NOINLINE void* operator new[](size_t n) { return ::operator new(n); }
PLATFORM_NOINLINE void operator delete[](void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete[](void* p, size_t) noexcept { std::free(p); }
