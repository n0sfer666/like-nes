#pragma once
#include <cstdint>

// Q16.16 fixed-point для ДЕТЕРМИНИРОВАННОЙ симуляции.
// Все операции определены и НАСЫЩАЮТ (clamp) при выходе за диапазон int32 —
// это убирает signed-overflow UB (в C++20 переполнение знакового СЛОЖЕНИЯ всё ещё UB,
// P0907 определил только сдвиги/конверсии). Насыщение детерминировано: одинаково на
// x86 и ARM. float-конверсии — ТОЛЬКО на границах (авторинг на входе, рендер на выходе),
// НИКОГДА внутри симуляции.
struct fix32 {
    int32_t raw;

    static constexpr int32_t SHIFT = 16;
    static constexpr int32_t ONE = 1 << SHIFT;

    static constexpr int32_t sat(int64_t v) {
        if (v > INT32_MAX) return INT32_MAX;
        if (v < INT32_MIN) return INT32_MIN;
        return static_cast<int32_t>(v);
    }

    constexpr fix32() : raw(0) {}
    constexpr explicit fix32(int32_t r, int) : raw(r) {}

    static constexpr fix32 from_raw(int32_t r) { return fix32(r, 0); }
    static constexpr fix32 from_int(int32_t v) { return from_raw(sat(static_cast<int64_t>(v) << SHIFT)); }
    // Только для авторинга/констант на границе — не использовать в горячем пути симуляции.
    // Клампим double ДО каста в int32 (иначе out-of-range double→int — UB ещё до sat()).
    static constexpr fix32 from_float(double v) {
        double s = v * ONE + (v >= 0 ? 0.5 : -0.5);
        if (s >= 2147483647.0) return from_raw(INT32_MAX);
        if (s <= -2147483648.0) return from_raw(INT32_MIN);
        return from_raw(static_cast<int32_t>(s));
    }

    constexpr int32_t to_int() const { return raw >> SHIFT; }
    // Только для рендера на границе.
    constexpr double to_double() const { return static_cast<double>(raw) / ONE; }

    constexpr fix32 operator+(fix32 o) const { return from_raw(sat(static_cast<int64_t>(raw) + o.raw)); }
    constexpr fix32 operator-(fix32 o) const { return from_raw(sat(static_cast<int64_t>(raw) - o.raw)); }
    constexpr fix32 operator-() const { return from_raw(sat(-static_cast<int64_t>(raw))); }
    constexpr fix32 operator*(fix32 o) const {
        int64_t p = static_cast<int64_t>(raw) * o.raw;
        return from_raw(sat(p >> SHIFT));
    }
    // Деление на ноль определено: насыщение по знаку числителя (никакого SIGFPE/UB).
    constexpr fix32 operator/(fix32 o) const {
        if (o.raw == 0) return from_raw(raw >= 0 ? INT32_MAX : INT32_MIN);
        return from_raw(sat((static_cast<int64_t>(raw) << SHIFT) / o.raw));
    }

    constexpr bool operator==(fix32 o) const { return raw == o.raw; }
    constexpr bool operator<(fix32 o) const { return raw < o.raw; }
};
