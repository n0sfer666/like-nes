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

Track track(const scene::Autoscroll& s) {
    const Vec2 half = graphics::view_zone_half();
    const fix32 lo = along_x(s) ? s.area.rect.x0 : s.area.rect.y0;
    const fix32 hi = along_x(s) ? s.area.rect.x1 : s.area.rect.y1;
    const fix32 h = along_x(s) ? half.x : half.y;
    const fix32 a = graphics::clamp_axis(lo, lo, hi, h);
    const fix32 b = graphics::clamp_axis(hi, lo, hi, h);
    return s.speed < fix32{} ? Track{b, a} : Track{a, b};
}

constexpr uint32_t SETTLE_SHARE = 8;

} // namespace

Ride ride_scroll(StageCamera& camera, Vec2 before, const scene::LevelObjects& level) {
    const Vec2 goal = clamp_level(camera.center, camera.arena, level, graphics::view_zone_half());
    graphics::settle_autoscroll(camera.autoscroll, camera.center, before, goal, SETTLE_SHARE);
    if (camera.autoscroll.ride >= level.autoscrolls.size()) return {};
    const scene::Autoscroll& s = level.autoscrolls[camera.autoscroll.ride];
    const Track t = track(s);
    const graphics::ScrollAxis axis = along_x(s) ? graphics::ScrollAxis::X : graphics::ScrollAxis::Y;
    const graphics::ScrollLane lane{axis, t.start, t.end, s.speed};
    return {graphics::ride_autoscroll(camera.autoscroll, camera.center, before, lane), s.axis};
}

} // namespace framework::brawl
