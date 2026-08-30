#include "viewport.hpp"

namespace framework::graphics {
namespace {

int64_t floor_div_i64(int64_t a, int64_t b) {
    const int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

fix32 axis_to_screen(fix32 world, fix32 center, fix32 scale, fix32 half) {
    return (world - center) * scale + half;
}

fix32 snap_screen(fix32 px) {
    return fix32::from_raw(
        fix32::sat(floor_div_i64(static_cast<int64_t>(px.raw), fix32::ONE) * fix32::ONE));
}

} // namespace

ViewportFault viewport_check(const Viewport& v) {
    if (v.pixels_per_unit <= 0) return VIEWPORT_PPU_NOT_POSITIVE;
    if (v.zoom.raw <= 0) return VIEWPORT_ZOOM_NOT_POSITIVE;
    if (v.screen_half.x.raw <= 0 || v.screen_half.y.raw <= 0) return VIEWPORT_SCREEN_EMPTY;
    return VIEWPORT_OK;
}

fix32 viewport_scale(const Viewport& v) {
    if (viewport_check(v) != VIEWPORT_OK) return fix32{};
    return v.zoom * fix32::from_int(v.pixels_per_unit);
}

Vec2 viewport_half_world(const Viewport& v) {
    const fix32 s = viewport_scale(v);
    if (s.raw == 0) return Vec2{};
    return {v.screen_half.x / s, v.screen_half.y / s};
}

Vec2 world_to_screen(const Viewport& v, Vec2 center, Vec2 world) {
    const fix32 s = viewport_scale(v);
    if (s.raw == 0) return Vec2{};
    return {axis_to_screen(world.x, center.x, s, v.screen_half.x),
            axis_to_screen(world.y, center.y, s, v.screen_half.y)};
}

Vec2 screen_to_world(const Viewport& v, Vec2 center, Vec2 screen) {
    const fix32 s = viewport_scale(v);
    if (s.raw == 0) return Vec2{};
    return {(screen.x - v.screen_half.x) / s + center.x,
            (screen.y - v.screen_half.y) / s + center.y};
}

Vec2 world_to_screen_snapped(const Viewport& v, Vec2 center, Vec2 world) {
    const Vec2 p = world_to_screen(v, center, world);
    if (viewport_scale(v).raw == 0) return p;
    return {snap_screen(p.x), snap_screen(p.y)};
}

bool viewport_is_pixel_exact(const Viewport& v) {
    const fix32 s = viewport_scale(v);
    return s.raw != 0 && (s.raw & (fix32::ONE - 1)) == 0;
}

} // namespace framework::graphics
