#include "camera_autoscroll.hpp"

#include <cstdio>

namespace {

using namespace framework;
using namespace framework::graphics;
int failures = 0;
fix32 px(int32_t v) { return fix32::from_int(v); }
void check(bool ok, const char* why) {
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", why); }
}

using Ride = bool (*)(Autoscroll&, Vec2&, Vec2, const ScrollLane&);
using Settle = void (*)(Autoscroll&, Vec2&, Vec2, Vec2, uint32_t);

bool crossing(Ride ride) {
    Autoscroll s;
    const ScrollLane lane{ScrollAxis::X, px(10), px(15), px(2)};
    Vec2 center{px(9), px(40)};
    if (ride(s, center, {px(8), px(40)}, lane) || s.riding) return false;
    center.x = px(12);
    if (!ride(s, center, {px(9), px(40)}, lane) || center.x != px(10) || !s.riding || s.ride != 0) return false;
    for (int t = 0; t < 3; ++t)
        if (!ride(s, center, center, lane)) return false;
    return center.x == px(15) && center.y == px(40) && !s.riding && s.ride == 1 && s.settle.x == px(2);
}

bool misses_crossing(Autoscroll& s, Vec2& c, Vec2 before, const ScrollLane& lane) {
    if (!s.riding && c.x != lane.start) return false;
    return ride_autoscroll(s, c, before, lane);
}

bool overruns(Autoscroll& s, Vec2& c, Vec2 before, const ScrollLane& lane) {
    const bool was = s.riding;
    const bool on = ride_autoscroll(s, c, before, lane);
    if (was) c.x = c.x + px(2);
    return on;
}

bool axes_and_skip(Ride ride) {
    Autoscroll s;
    const ScrollLane zero{ScrollAxis::X, px(10), px(15), fix32{}};
    Vec2 center{px(11), px(90)};
    if (ride(s, center, {px(9), px(90)}, zero) || s.riding || s.ride != 1 || center.x != px(11)) return false;
    const ScrollLane backward{ScrollAxis::Y, px(80), px(75), px(-3)};
    center.y = px(79);
    if (!ride(s, center, {px(90), px(81)}, backward) || center.y != px(80)) return false;
    if (!ride(s, center, center, backward) || center.y != px(77)) return false;
    if (!ride(s, center, center, backward) || center.y != px(75)) return false;
    return s.ride == 2 && !s.riding && s.settle.y == px(3) && center.x == px(11);
}

bool wrong_axis(Autoscroll& s, Vec2& c, Vec2 before, const ScrollLane& lane) {
    ScrollLane copy = lane;
    copy.axis = ScrollAxis::X;
    return ride_autoscroll(s, c, before, copy);
}

bool settlement(Settle settle) {
    Autoscroll s;
    s.settle = {px(2), px(3)};
    Vec2 c{px(80), px(21)};
    settle(s, c, {}, {px(80), px(21)}, 8);
    if (c.x != px(10) || c.y != px(3) || s.settle.x != px(2)) return false;
    settle(s, c, {}, {px(80), px(21)}, 4);
    if (c.x != px(20) || c.y != px(5) + fix32::from_raw(1 << 14)) return false;
    settle(s, c, {px(79), px(20)}, {px(80), px(21)}, 8);
    if (c.x != px(80) || c.y != px(21) || s.settle.x != fix32{} || s.settle.y != fix32{}) return false;
    s.settle = {px(1), px(1)};
    settle(s, c, {}, {px(80), px(21)}, 0);
    if (c.x != px(80) || c.y != px(21) || s.settle.x != fix32{} || s.settle.y != fix32{}) return false;
    c = {px(123), px(456)};
    settle(s, c, {}, {}, 8);
    return c.x == px(123) && c.y == px(456);
}

void fixed_share(Autoscroll& s, Vec2& c, Vec2 before, Vec2 goal, uint32_t) {
    settle_autoscroll(s, c, before, goal, 8);
}

void snap(Autoscroll& s, Vec2& c, Vec2, Vec2 goal, uint32_t) {
    c = goal;
    s.settle = {};
}

} // namespace

int main() {
    check(crossing(ride_autoscroll), "crossing starts at the lane origin, then fixed speed reaches its endpoint without overshoot");
    check(!crossing(misses_crossing) && !crossing(overruns), "control: missing a crossed start and overshooting are detected");
    check(axes_and_skip(ride_autoscroll) && !axes_and_skip(wrong_axis), "zero speed skips once; negative y travel keeps x unchanged");
    check(settlement(settle_autoscroll), "settle uses max(rate, gap/share), clamps to the goal and releases each axis");
    check(!settlement(fixed_share) && !settlement(snap), "control: a hardcoded share and instant return are detected");
    std::printf("framework-graphics-autoscroll: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
