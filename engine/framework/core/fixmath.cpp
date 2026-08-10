#include "fixmath.hpp"

namespace framework {
namespace {

// Сумма двух произведений raw-величин в Q32.32 с насыщением. Оба слагаемых укладываются в int64
// по построению (|raw| <= 2^31, произведение <= 2^62), а вот их сумма — уже нет: два предельных
// значения дают ровно 2^63. Переполнение здесь было бы UB, поэтому проверка на знак, а не
// надежда на то, что таких величин не бывает.
int64_t add_sat(int64_t p1, int64_t p2) {
    if (p2 > 0 && p1 > INT64_MAX - p2) return INT64_MAX;
    if (p2 < 0 && p1 < INT64_MIN - p2) return INT64_MIN;
    return p1 + p2;
}

int64_t sum_products(int32_t a1, int32_t b1, int32_t a2, int32_t b2) {
    return add_sat(static_cast<int64_t>(a1) * b1, static_cast<int64_t>(a2) * b2);
}

// Разность произведений отдельной функцией, а не `sum_products(a, b, -c, d)`: отрицание int32_t
// при значении INT32_MIN — знаковое переполнение, то есть UB, и поймал бы его только UBSan на
// вырожденном входе. Отрицается уже int64-произведение, где предельный int32 безопасен.
int64_t diff_products(int32_t a1, int32_t b1, int32_t a2, int32_t b2) {
    const int64_t p2 = static_cast<int64_t>(a2) * b2;
    // -INT64_MIN тоже переполняет, но p2 сюда прийти таким не может: |p2| <= 2^62.
    return add_sat(static_cast<int64_t>(a1) * b1, -p2);
}

} // namespace

int64_t isqrt64(int64_t v) {
    if (v <= 0) return 0;
    int64_t lo = 0, hi = 0x1'0000'0000LL;   // 2^32: квадрат уже вне int64
    while (lo < hi) {
        const int64_t mid = lo + (hi - lo + 1) / 2;
        if (mid <= v / mid) lo = mid; else hi = mid - 1;
    }
    return lo;
}

fix32 sqrt_fix(fix32 v) {
    if (v.raw <= 0) return fix32{};
    // Вход Q16.16, а корень нужен тоже в Q16.16: домножаем на 2^16 ДО корня, потому что
    // sqrt(v * 2^16 * 2^16) == sqrt(v) * 2^16. Сдвиг в int64 — без потери старших бит.
    return fix32::from_raw(static_cast<int32_t>(
        isqrt64(static_cast<int64_t>(v.raw) << fix32::SHIFT)));
}

fix32 dot(Vec2 a, Vec2 b) {
    return fix32::from_raw(
        fix32::sat(fix32::shift_down(sum_products(a.x.raw, b.x.raw, a.y.raw, b.y.raw))));
}

fix32 cross(Vec2 a, Vec2 b) {
    return fix32::from_raw(
        fix32::sat(fix32::shift_down(diff_products(a.x.raw, b.y.raw, a.y.raw, b.x.raw))));
}

fix32 length(Vec2 v) {
    const int64_t sum = sum_products(v.x.raw, v.x.raw, v.y.raw, v.y.raw);
    return fix32::from_raw(static_cast<int32_t>(fix32::sat(isqrt64(sum))));
}

fix32 length_sq(Vec2 v) {
    return fix32::from_raw(fix32::sat(sum_products(v.x.raw, v.x.raw, v.y.raw, v.y.raw) >> fix32::SHIFT));
}

fix32 normalize(Vec2 v, Vec2& out) {
    const int64_t len2 = sum_products(v.x.raw, v.x.raw, v.y.raw, v.y.raw);
    if (len2 <= 0) {
        out = Vec2{};
        return fix32{};
    }
    // Восемь дополнительных бит у корня: sqrt(len2 << 16) == sqrt(len2) * 256. Сдвиг применим не
    // всегда — len2 доходит до 2^63, — но там, где он не влезает, вектор заведомо длиннее 128
    // юнитов, и относительная ошибка округления корня уже пренебрежима.
    const bool precise = len2 < (INT64_C(1) << 46);
    const int64_t root = precise ? isqrt64(len2 << fix32::SHIFT) : isqrt64(len2);
    const int extra = precise ? 8 : 0;
    out = {fix32::from_raw(fix32::sat((static_cast<int64_t>(v.x.raw) << (fix32::SHIFT + extra)) / root)),
           fix32::from_raw(fix32::sat((static_cast<int64_t>(v.y.raw) << (fix32::SHIFT + extra)) / root))};
    return fix32::from_raw(fix32::sat(root >> extra));
}

Vec2 clamp_length(Vec2 v, fix32 limit) {
    if (limit.raw <= 0) return Vec2{};
    const int64_t len2 = sum_products(v.x.raw, v.x.raw, v.y.raw, v.y.raw);
    const int64_t lim = limit.raw;
    // Сравнение квадратов, а не длин: корень здесь понадобился бы только чтобы его же и сравнить,
    // и именно его округление вниз пропускало бы вектор, стоящий на волосок выше потолка.
    if (len2 <= lim * lim) return v;
    const int64_t root = isqrt64(len2);
    if (root <= 0) return v;
    return {fix32::from_raw(fix32::sat(static_cast<int64_t>(v.x.raw) * lim / root)),
            fix32::from_raw(fix32::sat(static_cast<int64_t>(v.y.raw) * lim / root))};
}

} // namespace framework
