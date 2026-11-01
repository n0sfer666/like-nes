#include "camera_autoscroll.hpp"

namespace framework::graphics {

namespace {

fix32& on_axis(Vec2& v, ScrollAxis axis) { return axis == ScrollAxis::X ? v.x : v.y; }

fix32 toward(fix32 from, fix32 to, fix32 step) {
    return from < to ? min_fix(from + step, to) : max_fix(from - step, to);
}

void settle_axis(fix32& rate, fix32 from, fix32 target, fix32& center, uint32_t share) {
    if (rate == fix32{}) return;
    const fix32 gap = abs_fix(target - from);
    center = toward(from, target, max_fix(rate, fix32::from_raw(static_cast<int32_t>(gap.raw / share))));
    if (center == target) rate = fix32{};
}

} // namespace

void settle_autoscroll(Autoscroll& state, Vec2& center, Vec2 before, Vec2 goal, uint32_t share) {
    if (share == 0) share = 1;
    settle_axis(state.settle.x, before.x, goal.x, center.x, share);
    settle_axis(state.settle.y, before.y, goal.y, center.y, share);
}

bool ride_autoscroll(Autoscroll& state, Vec2& center, Vec2 before, const ScrollLane& lane) {
    fix32& c = on_axis(center, lane.axis);
    if (state.riding) {
        state.scroll = toward(state.scroll, lane.end, abs_fix(lane.speed));
    } else {
        const fix32 previous = on_axis(before, lane.axis);
        if (clamp_fix(lane.start, min_fix(previous, c), max_fix(previous, c)) != lane.start) return false;
        if (lane.speed == fix32{}) {
            ++state.ride;
            return false;
        }
        state.riding = true;
        state.scroll = lane.start;
    }
    c = state.scroll;
    if (state.scroll == lane.end) {
        state.riding = false;
        on_axis(state.settle, lane.axis) = abs_fix(lane.speed);
        ++state.ride;
    }
    return true;
}

} // namespace framework::graphics
