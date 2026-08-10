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
    // Возврат произведения в Q16.16 — УСЕЧЕНИЕМ К НУЛЮ, а не арифметическим сдвигом. Разница видна
    // только на отрицательных и только в младшем разряде, и ровно поэтому она была тихой: `>>`
    // округляет к минус бесконечности, то есть произведение по модулю меньше кванта даёт -1 там,
    // где такое же положительное даёт 0. Смещение не гасится, оно КОПИТСЯ: тело со скоростью
    // -55 raw (0.0008 юнита в секунду, вчетверо ниже разрешения позиции) уезжало на 1 raw каждый
    // кадр, то есть на 0.001 юнита в секунду вечного дрейфа в одну сторону; угловая скорость в один
    // младший разряд по той же причине поворачивала лежащий ящик на целый квант за кадр вместо
    // шестидесятой его доли, и башня из шести ящиков за две минуты укладывалась в один слой.
    //
    // Усечение к нулю делает подквантовое произведение нулём с ОБЕИХ сторон и заодно совпадает с
    // `operator/`: там целое деление усекает к нулю, то есть до этого умножение и деление
    // округляли в разные стороны. Симметрия по знаку — не эстетика: без неё зеркальная сцена
    // считается не как зеркало исходной, и `axis_terms` зря обещает обратное.
    //
    // ПРЕДУСЛОВИЕ: `p > INT64_MIN`. Ровно на INT64_MIN отрицание `-p` — знаковое переполнение, то
    // есть UB, и поймал бы его только UBSan на вырожденном входе. Кто его держит, по классам
    // вызывающих: `operator*` ниже — сам, множители int32, |p| <= 2^62; `dot`/`cross`
    // (`fixmath.cpp`) — НЕ конструкцией, а областью значений, потому что их `add_sat` в насыщении
    // возвращает как раз INT64_MIN: слагаемые пришли из клампов мира (позиция <= WORLD_HALF,
    // скорость <= MAX_SPEED), то есть |raw| <= 2^29 и сумма не выходит за 2^59; `share_of`
    // (`impulse.hpp`) — потолком накопленного, MAX_ACCUM = 2^28 против доли в int32.
    static constexpr int64_t shift_down(int64_t p) {
        return p < 0 ? -((-p) >> SHIFT) : (p >> SHIFT);
    }

    constexpr fix32 operator*(fix32 o) const {
        int64_t p = static_cast<int64_t>(raw) * o.raw;
        return from_raw(sat(shift_down(p)));
    }
    // Деление на ноль определено: насыщение по знаку числителя (никакого SIGFPE/UB).
    constexpr fix32 operator/(fix32 o) const {
        if (o.raw == 0) return from_raw(raw >= 0 ? INT32_MAX : INT32_MIN);
        return from_raw(sat((static_cast<int64_t>(raw) << SHIFT) / o.raw));
    }

    constexpr bool operator==(fix32 o) const { return raw == o.raw; }
    constexpr bool operator<(fix32 o) const { return raw < o.raw; }
};
