#pragma once
#include <cstdint>

// Q16.16 fixed-point для ДЕТЕРМИНИРОВАННОЙ симуляции.
// Хранилище int32, промежуточные mul/div через int64 (без переполнения в рабочем диапазоне).
// Целочисленные операции бит-в-бит идентичны на x86 и ARM — это и есть гарантия детерминизма.
// float-конверсии — ТОЛЬКО на границах (константы-авторинг на входе, рендер на выходе),
// НИКОГДА внутри симуляции.
struct fix32 {
    int32_t raw;

    static constexpr int32_t SHIFT = 16;
    static constexpr int32_t ONE = 1 << SHIFT;

    constexpr fix32() : raw(0) {}
    constexpr explicit fix32(int32_t r, int) : raw(r) {}

    static constexpr fix32 from_raw(int32_t r) { return fix32(r, 0); }
    static constexpr fix32 from_int(int32_t v) { return fix32(v << SHIFT, 0); }
    // Только для авторинга/констант на границе — не использовать в горячем пути симуляции.
    static constexpr fix32 from_float(double v) {
        return fix32(static_cast<int32_t>(v * ONE + (v >= 0 ? 0.5 : -0.5)), 0);
    }

    constexpr int32_t to_int() const { return raw >> SHIFT; }
    // Только для рендера на границе.
    constexpr double to_double() const { return static_cast<double>(raw) / ONE; }

    constexpr fix32 operator+(fix32 o) const { return from_raw(raw + o.raw); }
    constexpr fix32 operator-(fix32 o) const { return from_raw(raw - o.raw); }
    constexpr fix32 operator-() const { return from_raw(-raw); }
    constexpr fix32 operator*(fix32 o) const {
        int64_t p = static_cast<int64_t>(raw) * o.raw;
        return from_raw(static_cast<int32_t>(p >> SHIFT));
    }
    constexpr fix32 operator/(fix32 o) const {
        int64_t n = (static_cast<int64_t>(raw) << SHIFT);
        return from_raw(static_cast<int32_t>(n / o.raw));
    }

    constexpr bool operator==(fix32 o) const { return raw == o.raw; }
    constexpr bool operator<(fix32 o) const { return raw < o.raw; }
};
