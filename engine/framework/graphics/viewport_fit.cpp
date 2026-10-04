#include "viewport_fit.hpp"

#include <algorithm>

namespace framework::graphics {
namespace {

ScreenRect centered(PixelSize screen, uint64_t w, uint64_t h) {
    const auto cw = static_cast<uint32_t>(std::min<uint64_t>(w, screen.w));
    const auto ch = static_cast<uint32_t>(std::min<uint64_t>(h, screen.h));
    return {static_cast<int32_t>((screen.w - cw) / 2), static_cast<int32_t>((screen.h - ch) / 2), cw,
            ch};
}

uint32_t visible_side(uint32_t screen, uint32_t k, uint32_t zone, uint32_t limit) {
    const uint64_t seen = (uint64_t{screen} + k - 1) / k;
    // Предел не уже зоны: зона, которую предел режет, показывала бы игроку меньше, чем видит
    // симуляция, на любом экране, а не только в маленьком окне.
    return static_cast<uint32_t>(std::min<uint64_t>(seen, std::max(zone, limit)));
}

void push(ViewportFit& f, int64_t x, int64_t y, int64_t w, int64_t h) {
    if (w <= 0 || h <= 0) return;
    f.strips[f.strip_count++] = {static_cast<int32_t>(x), static_cast<int32_t>(y),
                                 static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

} // namespace

Vec2 view_zone_half(PixelSize zone) {
    return {fix32::from_raw(fix32::sat(int64_t{zone.w} * fix32::ONE / 2)),
            fix32::from_raw(fix32::sat(int64_t{zone.h} * fix32::ONE / 2))};
}

ViewportFit viewport_fit(PixelSize screen, PixelSize zone) {
    ViewportFit f;
    if (zone.w == 0 || zone.h == 0) return f;
    f.scale = std::max(1u, std::min(screen.w / zone.w, screen.h / zone.h));
    const uint64_t k = f.scale;
    f.view.screen_half = {fix32::from_int(static_cast<int32_t>(screen.w)) / fix32::from_int(2),
                          fix32::from_int(static_cast<int32_t>(screen.h)) / fix32::from_int(2)};
    f.view.zoom = fix32::from_int(static_cast<int32_t>(f.scale));
    f.visible = {visible_side(screen.w, f.scale, zone.w, VIEW_LIMIT.w),
                 visible_side(screen.h, f.scale, zone.h, VIEW_LIMIT.h)};
    f.shown = centered(screen, f.visible.w * k, f.visible.h * k);
    f.zone = centered(screen, zone.w * k, zone.h * k);
    f.cropped = screen.w < zone.w || screen.h < zone.h;

    const int64_t zx = f.zone.x, zy = f.zone.y, zw = f.zone.w, zh = f.zone.h;
    const int64_t sw = screen.w, sh = screen.h;
    push(f, 0, 0, sw, zy);
    push(f, 0, zy + zh, sw, sh - zy - zh);
    push(f, 0, zy, zx, zh);
    push(f, zx + zw, zy, sw - zx - zw, zh);
    return f;
}

} // namespace framework::graphics
