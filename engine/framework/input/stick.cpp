#include "stick.hpp"

namespace framework::input {
namespace {

const fix32 ONE = fix32::from_int(1);

// Мёртвая зона и порог насыщения приходят из ассета, то есть из данных, которые правит человек.
// Развёрнутый диапазон — не паранойя: dz >= outer обращает знаменатель нормализации в ноль, и
// кривая ушла бы в насыщение на первом же кадре.
void sanitize(fix32& dz, fix32& outer) {
    if (dz.raw < 0) dz = fix32{};
    if (!(outer.raw > 0)) outer = ONE;
    if (!(dz < outer)) dz = fix32::from_raw(outer.raw - 1);
}

} // namespace

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
    const fix32 len = length(v);
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
