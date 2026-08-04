#include "stick.hpp"

namespace framework::input {
namespace {

const fix32 ONE = fix32::from_int(1);

fix32 abs_fix(fix32 v) { return v.raw < 0 ? -v : v; }

// Мёртвая зона и порог насыщения приходят из ассета, то есть из данных, которые правит человек.
// Развёрнутый диапазон — не паранойя: dz >= outer обращает знаменатель нормализации в ноль, и
// кривая ушла бы в насыщение на первом же кадре.
void sanitize(fix32& dz, fix32& outer) {
    if (dz.raw < 0) dz = fix32{};
    if (!(outer.raw > 0)) outer = ONE;
    if (!(dz < outer)) dz = fix32::from_raw(outer.raw - 1);
}

// Модуль вектора в Q16.16 без промежуточного fix32-умножения: x*y там насыщает и, что важнее,
// теряет младшие 16 бит, а корень эту потерю усиливает — на медленном наклоне стика это ровно
// те ступеньки, из-за которых персонаж «дёргается» вместо плавного разгона.
// floor(sqrt(v)) на целых: двоичный поиск, потому что он одинаков всюду и не зависит от
// доступности 128-битного умножения. Сравнение через деление, а не mid*mid: квадрат кандидата
// переполняет int64 на верхней границе диапазона.
int64_t isqrt64(int64_t v) {
    if (v <= 0) return 0;
    int64_t lo = 0, hi = 0x1'0000'0000LL;   // 2^32: квадрат уже вне int64
    while (lo < hi) {
        const int64_t mid = lo + (hi - lo + 1) / 2;
        if (mid <= v / mid) lo = mid; else hi = mid - 1;
    }
    return lo;
}

int64_t length_raw(fix32 x, fix32 y) {
    const int64_t xx = static_cast<int64_t>(x.raw) * x.raw;
    const int64_t yy = static_cast<int64_t>(y.raw) * y.raw;
    int64_t sum = xx + yy;
    if (sum < 0) sum = INT64_MAX;   // насыщение вместо UB: два предельных raw дают ровно 2^63
    // Корень из Q32.32 даёт Q16.16 прямо: sqrt(v * 2^32) == sqrt(v) * 2^16.
    return isqrt64(sum);
}

} // namespace

fix32 sqrt_fix(fix32 v) {
    if (v.raw <= 0) return fix32{};
    return fix32::from_raw(static_cast<int32_t>(isqrt64(static_cast<int64_t>(v.raw) << fix32::SHIFT)));
}

fix32 apply_curve(fix32 t, uint32_t exp) {
    if (exp <= 1) return t;
    fix32 out = t;
    for (uint32_t i = 1; i < exp; ++i) out = out * t;
    return out;
}

fix32 axial(fix32 v, const StickShape& s) {
    fix32 dz = s.deadzone, outer = s.outer;
    sanitize(dz, outer);
    const fix32 mag = abs_fix(v);
    if (!(dz < mag)) return fix32{};
    fix32 t = (mag - dz) / (outer - dz);
    if (ONE < t) t = ONE;
    t = apply_curve(t, s.curve_exp);
    return v.raw < 0 ? -t : t;
}

Vec2 radial(Vec2 v, const StickShape& s) {
    fix32 dz = s.deadzone, outer = s.outer;
    sanitize(dz, outer);
    const int64_t len_raw = length_raw(v.x, v.y);
    const fix32 len = fix32::from_raw(len_raw > INT32_MAX ? INT32_MAX : static_cast<int32_t>(len_raw));
    if (!(dz < len)) return {};

    fix32 t = (len - dz) / (outer - dz);
    if (ONE < t) t = ONE;
    t = apply_curve(t, s.curve_exp);

    // Направление сохраняется, длина заменяется на обработанную: делим на исходный модуль, а не
    // клампим покомпонентно — покомпонентный кламп превратил бы круг в квадрат обратно.
    const fix32 scale = t / len;
    return {v.x * scale, v.y * scale};
}

fix32 trigger(fix32 v, fix32 threshold) {
    if (v.raw < 0) return fix32{};
    fix32 dz = threshold, outer = ONE;
    sanitize(dz, outer);
    if (!(dz < v)) return fix32{};
    fix32 t = (v - dz) / (outer - dz);
    if (ONE < t) t = ONE;
    return t;
}

} // namespace framework::input
