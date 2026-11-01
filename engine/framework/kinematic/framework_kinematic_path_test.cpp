#include "framework_kinematic_fixture.hpp"

namespace {

using namespace framework;
using namespace framework::kinematic;
using namespace framework::kinematic::test;
using Advance = void (*)(const scene::Path&, PathFollow&);
using Point = Vec2 (*)(const scene::Path&, const PathFollow&);

bool at(Vec2 v, int32_t x, int32_t y) { return abs_fix(v.x - px(x)).raw <= 10 && abs_fix(v.y - px(y)).raw <= 10; }

bool course(Advance advance, Point point) {
    const scene::Path loop = path({{px(0), px(0)}, {px(10), px(0)}, {px(10), px(6)}, {px(0), px(6)}}, 3, true);
    PathFollow f;
    for (int t = 0; t < 4; ++t) advance(loop, f);
    if (!at(point(loop, f), 10, 2) || f.segment != 1 || f.traveled != px(2)) return false;
    for (int t = 0; t < 7; ++t) advance(loop, f);
    if (!at(point(loop, f), 1, 0) || f.segment != 0 || f.direction != 1) return false;
    const scene::Path pendulum = path({{px(0), px(0)}, {px(10), px(0)}, {px(10), px(6)}}, 4);
    f = {};
    for (int t = 0; t < 4; ++t) advance(pendulum, f);
    if (!at(point(pendulum, f), 10, 6) || f.direction != -1 || f.traveled != fix32{}) return false;
    advance(pendulum, f);
    advance(pendulum, f);
    if (!at(point(pendulum, f), 8, 0) || f.direction != -1) return false;
    advance(pendulum, f);
    advance(pendulum, f);
    return at(point(pendulum, f), 0, 0) && f.segment == 0 && f.direction == 1;
}

void loses_remainder(const scene::Path& p, PathFollow& f) {
    advance_path(p, f);
    f.traveled = fix32{};
}

void loses_turn(const scene::Path& p, PathFollow& f) {
    advance_path(p, f);
    f.direction = 1;
}

Vec2 first_point(const scene::Path& p, const PathFollow&) { return p.points[0]; }

bool short_courses(Advance advance) {
    const scene::Path fast = path({{px(0), px(0)}, {px(4), px(0)}}, 20);
    PathFollow f{0, 1, px(2)};
    advance(fast, f);
    if (!at(path_point(fast, f), 2, 0) || f.segment != 1 || f.direction != -1) return false;
    const scene::Path skip = path({{}, {}, {px(10), fix32{}}}, 4);
    f = {};
    advance(skip, f);
    if (!at(path_point(skip, f), 4, 0)) return false;
    const scene::Path same = path({{px(5), px(5)}, {px(5), px(5)}, {px(5), px(5)}}, 4);
    f = {};
    advance(same, f);
    if (!at(path_point(same, f), 5, 5) || f.traveled != fix32{}) return false;
    const scene::Path one = path({{px(7), px(9)}}, 4);
    advance(one, f);
    if (!at(path_point(one, f), 7, 9)) return false;
    const scene::Path zero = path({{}, {px(10), fix32{}}}, 0);
    advance(zero, f);
    return at(path_point(zero, f), 0, 0);
}

void drops_fast_laps(const scene::Path& p, PathFollow& f) {
    if (px(16) < p.speed) return;
    advance_path(p, f);
}

} // namespace

int main() {
    check(course(advance_path, path_point), "loop closing edge, leftover and both turns retain their state and fixed-point position");
    check(!course(loses_remainder, path_point) && !course(loses_turn, path_point) && !course(advance_path, first_point),
          "control: lost travel, direction and geometry are detected");
    check(short_courses(advance_path), "multi-lap speed, zero-length segments and stationary paths terminate correctly");
    check(!short_courses(drops_fast_laps), "control: dropping a speed above the cycle loses its remainder");
    return verdict("framework-kinematic-path");
}
