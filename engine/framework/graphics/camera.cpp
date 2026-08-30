#include "camera.hpp"

namespace framework::graphics {
namespace {

fix32 deadzone_want(fix32 center, fix32 target, fix32 half) {
    const fix32 d = target - center;
    if (abs_fix(d).raw <= half.raw) return center;
    return d.raw > 0 ? target - half : target + half;
}

fix32 approach(fix32 v, fix32 goal, fix32 rate) {
    if (rate.raw <= 0) return goal;
    const fix32 d = goal - v;
    if (abs_fix(d).raw <= rate.raw) return goal;
    return d.raw > 0 ? v + rate : v - rate;
}

fix32 clamp_axis(fix32 v, fix32 lo, fix32 hi, fix32 half) {
    const fix32 low = lo + half;
    const fix32 high = hi - half;
    // Уровень уже видимой области: удержать камеру внутри нельзя, и отрезок разрешённых центров
    // выворачивается наизнанку. Тогда середина уровня — а `clamp_fix` по вывернутому отрезку молча
    // отдал бы один из краёв и прижал бы вид к нему, оставив с другой стороны пустоту.
    if (high < low) {
        return fix32::from_raw(
            fix32::sat((static_cast<int64_t>(lo.raw) + static_cast<int64_t>(hi.raw)) / 2));
    }
    return clamp_fix(v, low, high);
}

// Шум тряски. Своя функция, а не `hash_mix.hpp` из физики: та константа принадлежит ГОЛДЕНАМ, и
// общая с ними означала бы, что подкрутка свёртки меняет вид игры.
int32_t shake_noise(uint32_t seed, uint64_t tick, uint32_t axis) {
    uint32_t h = seed * 0x9e3779b9u + static_cast<uint32_t>(tick) * 0x85ebca6bu + axis * 0xc2b2ae35u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    // Q16.16 в [-1, 1): семнадцать младших разрядов минус единица.
    return static_cast<int32_t>(h & 0x1ffffu) - fix32::ONE;
}

int64_t floor_div(int64_t a, int64_t b) {
    const int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

// Привязка к пиксельной сетке ОКРУГЛЕНИЕМ ВНИЗ, а не к нулю. Усечение к нулю ставит шов ровно на
// нуле: два соседних положения по разные стороны от него попадают в один пиксель, и камера,
// проезжающая через ноль, дёргается там на целый пиксель — ровно то дрожание, которое режим
// обязан убрать.
fix32 snap_pixel(fix32 v, int32_t ppu) {
    const int64_t px = floor_div(static_cast<int64_t>(v.raw) * ppu, fix32::ONE);
    return fix32::from_raw(fix32::sat(px * fix32::ONE / ppu));
}

} // namespace

void camera_follow(Camera& c, const CameraConfig& cfg, Vec2 target, int32_t facing) {
    Vec2 want = target;
    if ((cfg.policies & CAMERA_DEADZONE) != 0) {
        want.x = deadzone_want(c.center.x, target.x, cfg.dead_half.x);
        want.y = deadzone_want(c.center.y, target.y, cfg.dead_half.y);
    }
    if ((cfg.policies & CAMERA_LOOK_AHEAD) != 0) {
        const fix32 goal =
            facing > 0 ? cfg.look_ahead : (facing < 0 ? -cfg.look_ahead : fix32{});
        c.look = approach(c.look, goal, cfg.look_rate);
        want.x = want.x + c.look;
    }
    if ((cfg.policies & CAMERA_BOUNDS) != 0) {
        want.x = clamp_axis(want.x, cfg.bounds.min_x, cfg.bounds.max_x, cfg.half_view.x);
        want.y = clamp_axis(want.y, cfg.bounds.min_y, cfg.bounds.max_y, cfg.half_view.y);
    }
    Vec2 step = want - c.center;
    if ((cfg.policies & CAMERA_SPEED_LIMIT) != 0 && cfg.max_speed.raw > 0) {
        step = clamp_length(step, cfg.max_speed);
    }
    c.center = c.center + step;
    if ((cfg.policies & CAMERA_BOUNDS) != 0) {
        c.center.x = clamp_axis(c.center.x, cfg.bounds.min_x, cfg.bounds.max_x, cfg.half_view.x);
        c.center.y = clamp_axis(c.center.y, cfg.bounds.min_y, cfg.bounds.max_y, cfg.half_view.y);
    }
    if (c.shake_ticks > 0) --c.shake_ticks;
}

void camera_shake(Camera& c, uint32_t ticks, fix32 amp, uint32_t seed) {
    c.shake_ticks = ticks;
    c.shake_total = ticks;
    c.shake_amp = amp;
    c.shake_seed = seed;
}

Vec2 camera_shake_offset(const Camera& c, uint64_t tick) {
    if (c.shake_ticks == 0 || c.shake_total == 0) return Vec2{};
    // Затухание линейное по остатку заказа: обрыв на полной амплитуде читается как второй удар,
    // которого в игре не было.
    const fix32 amp = fix32::from_raw(fix32::sat(static_cast<int64_t>(c.shake_amp.raw) *
                                                 c.shake_ticks / c.shake_total));
    return Vec2{amp * fix32::from_raw(shake_noise(c.shake_seed, tick, 0)),
                amp * fix32::from_raw(shake_noise(c.shake_seed, tick, 1))};
}

Vec2 camera_layer_center(const Camera& c, const CameraConfig& cfg, uint64_t tick, fix32 parallax) {
    Vec2 v = (c.center + camera_shake_offset(c, tick)) * parallax;
    if ((cfg.policies & CAMERA_PIXEL_PERFECT) != 0 && cfg.pixels_per_unit > 0) {
        v.x = snap_pixel(v.x, cfg.pixels_per_unit);
        v.y = snap_pixel(v.y, cfg.pixels_per_unit);
    }
    return v;
}

Vec2 camera_view_center(const Camera& c, const CameraConfig& cfg, uint64_t tick) {
    return camera_layer_center(c, cfg, tick, fix32::from_int(1));
}

} // namespace framework::graphics
