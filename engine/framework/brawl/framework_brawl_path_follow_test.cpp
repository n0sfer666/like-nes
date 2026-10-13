#include "framework_brawl_check.hpp"
#include "framework_brawl_path_fixture.hpp"
#include "framework_brawl_section_fixture.hpp"
#include "path_follow.hpp"

// Следование по `path` (спека #25, В8а): петля и маятник с остатком хода через вершину в обе
// стороны, вырожденные пути и ход больше обхода за тик.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::Vec2;
namespace scene = framework::scene;

bool near(Vec2 v, int32_t x, int32_t y) { return about(v.x, x) && about(v.y, y); }

Vec2 after(const scene::Path& p, PathFollow& f, uint32_t ticks) {
    for (uint32_t t = 0; t < ticks; ++t) advance_path(p, f);
    return path_point(p, f);
}

void loop_wraps() {
    const scene::Path p = path({{0, 0}, {10, 0}, {10, 6}, {0, 6}}, 3, true);
    PathFollow f;
    check(near(after(p, f, 4), 10, 2), "the leftover past a vertex is lost on a loop");
    check(near(after(p, f, 2), 8, 6), "a loop does not turn at its third vertex");
    check(near(after(p, f, 4), 0, 2), "a loop does not run its closing edge");
    check(near(after(p, f, 1), 1, 0) && f.segment == 0 && f.direction == 1, "a loop does not wrap to its first vertex");
}

void pendulum_turns() {
    const scene::Path p = path({{0, 0}, {10, 0}, {10, 6}}, 4, false);
    PathFollow f;
    check(near(after(p, f, 3), 10, 2), "the leftover past a vertex is lost on the way out");
    check(near(after(p, f, 1), 10, 6) && f.direction == -1, "a pendulum does not turn at its last vertex");
    check(near(after(p, f, 1), 10, 2), "a pendulum does not come back down its last edge");
    check(near(after(p, f, 1), 8, 0) && f.direction == -1, "the leftover past a vertex is lost on the way back");
    check(near(after(p, f, 2), 0, 0) && f.segment == 0 && f.direction == 1, "a pendulum does not turn at its first vertex");
    check(near(after(p, f, 1), 4, 0), "a pendulum does not set out again");
    PathFollow still;
    check(near(after(path({{5, 5}}, 4, false), still, 3), 5, 5), "a one-point path moves");
}

void pendulum_of_two_turns() {
    const scene::Path p = path({{0, 0}, {10, 0}}, 4, false);
    PathFollow f;
    check(near(after(p, f, 3), 8, 0) && f.segment == 1 && f.direction == -1, "a two-point pendulum does not turn");
    check(near(after(p, f, 2), 0, 0) && f.segment == 0 && f.direction == 1, "a two-point pendulum does not come back");
    check(near(after(p, f, 1), 4, 0), "a two-point pendulum does not set out again");
}

// Путь нулевой длины стоит; ход больше обхода за тик берётся по модулю цикла, а не обрывается:
// мелкий путь на большой скорости едет, а не стоит в вершине.
void degenerate_paths_stand() {
    PathFollow same;
    check(near(after(path({{5, 5}, {5, 5}, {5, 5}}, 4, false), same, 3), 5, 5) && same.traveled.raw == 0,
          "a path of coincident vertices moves or keeps travel");
    PathFollow skip;
    check(near(after(path({{0, 0}, {0, 0}, {10, 0}}, 4, false), skip, 1), 4, 0),
          "a zero-length segment eats the travel");
    PathFollow fast{0, 1, at(2)};
    check(near(after(path({{0, 0}, {4, 0}}, 20, false), fast, 1), 2, 0) && fast.segment == 1 && fast.direction == -1,
          "a pendulum run past two laps a tick loses its leftover");
    const scene::Path square = path({{0, 0}, {4, 0}, {4, 4}, {0, 4}}, 18, true);
    PathFollow lap;
    check(near(after(square, lap, 1), 2, 0) && near(after(square, lap, 1), 4, 0) && lap.segment == 1,
          "a loop run past a lap a tick loses its leftover");
    PathFollow parked;
    check(near(after(path({{0, 0}, {10, 0}}, 0, false), parked, 5), 0, 0), "a zero-speed path moves");
}

} // namespace

int main() {
    loop_wraps();
    pendulum_turns();
    pendulum_of_two_turns();
    degenerate_paths_stand();
    return verdict("framework-brawl-path-follow");
}
