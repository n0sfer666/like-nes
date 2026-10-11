#include "autoscroll_step.hpp"

#include "camera.hpp"
#include "level_clamp.hpp"
#include "viewport_fit.hpp"

namespace framework::brawl {

namespace {

struct Track {
    fix32 start, end;
};

bool along_x(const scene::Autoscroll& s) { return s.axis == scene::ScrollAxis::X; }

fix32& on_axis(Vec2& v, const scene::Autoscroll& s) { return along_x(s) ? v.x : v.y; }

Track track(const scene::Autoscroll& s) {
    const Vec2 half = graphics::view_zone_half();
    const fix32 lo = along_x(s) ? s.area.rect.x0 : s.area.rect.y0;
    const fix32 hi = along_x(s) ? s.area.rect.x1 : s.area.rect.y1;
    const fix32 h = along_x(s) ? half.x : half.y;
    const fix32 a = graphics::clamp_axis(lo, lo, hi, h);
    const fix32 b = graphics::clamp_axis(hi, lo, hi, h);
    return s.speed < fix32{} ? Track{b, a} : Track{a, b};
}

bool between(fix32 v, fix32 a, fix32 b) { return clamp_fix(v, min_fix(a, b), max_fix(a, b)) == v; }

fix32 toward(fix32 from, fix32 to, fix32 step) {
    return from < to ? min_fix(from + step, to) : max_fix(from - step, to);
}

constexpr int32_t SETTLE_SHARE = 8;

void settle_axis(fix32& rate, fix32 from, fix32 target, fix32& c) {
    if (rate == fix32{}) return;
    const fix32 gap = abs_fix(target - from);
    c = toward(from, target, max_fix(rate, fix32::from_raw(gap.raw / SETTLE_SHARE)));
    if (c == target) rate = fix32{};
}

} // namespace

Ride ride_scroll(StageCamera& camera, Vec2 before, const scene::LevelObjects& level) {
    const Vec2 goal = clamp_level(camera.center, camera.arena, level, graphics::view_zone_half());
    settle_axis(camera.settle.x, before.x, goal.x, camera.center.x);
    settle_axis(camera.settle.y, before.y, goal.y, camera.center.y);
    if (camera.ride >= level.autoscrolls.size()) return {};
    const scene::Autoscroll& s = level.autoscrolls[camera.ride];
    const Track t = track(s);
    fix32& c = on_axis(camera.center, s);
    if (camera.riding) {
        camera.scroll = toward(camera.scroll, t.end, abs_fix(s.speed));
    } else {
        if (!between(t.start, on_axis(before, s), c)) return {};
        if (s.speed == fix32{}) {
            ++camera.ride;
            return {};
        }
        camera.riding = true;
        camera.scroll = t.start;
    }
    c = camera.scroll;
    if (camera.scroll == t.end) {
        camera.riding = false;
        on_axis(camera.settle, s) = abs_fix(s.speed);
        ++camera.ride;
    }
    return {true, s.axis};
}

} // namespace framework::brawl
