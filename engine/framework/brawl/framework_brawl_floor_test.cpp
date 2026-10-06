#include "depth_floor.hpp"
#include "framework_brawl_check.hpp"

namespace {

using namespace framework::brawl;
using test::check;

constexpr fix32 px(int32_t v) { return fix32::from_int(v); }
constexpr fix32 raw(int32_t v) { return fix32::from_raw(v); }

DepthFloor walled() {
    DepthFloor f;
    f.band = FloorRect{px(0), px(0), px(100), px(50)};
    f.add_wall(FloorRect{px(40), px(10), px(60), px(30)});
    return f;
}

DepthBody moved(fix32 x, fix32 z, fix32 vx, fix32 vz) {
    DepthBody b{x, z, fix32{}, vx, vz, fix32{}};
    slide(walled(), b);
    return b;
}

void test_the_wall_stops_on_its_edge_from_both_sides() {
    const DepthBody cut = moved(px(40) - raw(1), px(20), raw(2), fix32{});
    check(cut.x == px(40), "from the left one raw short of the edge the step stops on x0");
    check(cut.vx == fix32{}, "the cut x step loses its speed");
    check(moved(px(40) - raw(1), px(20), raw(1), fix32{}).x == px(40),
          "a step that ends exactly on x0 is not cut");
    check(moved(px(60) + raw(1), px(20), raw(-2), fix32{}).x == px(60),
          "from the right the step stops on x1");
    check(moved(px(50), px(10) - raw(1), fix32{}, raw(2)).z == px(10), "from below on z0");
    check(moved(px(50), px(30) + raw(1), fix32{}, raw(-2)).z == px(30), "from above on z1");
    check(moved(px(30), px(20), px(50), fix32{}).x == px(40), "a long step does not tunnel");
    check(moved(px(40), px(20), raw(1), fix32{}).x == px(40), "standing on x0 the wall still holds");
    check(moved(px(40), px(20), raw(-1), fix32{}).x == px(40) - raw(1), "and lets him back off");
}

void test_the_edge_line_is_part_of_the_wall() {
    check(moved(px(30), px(10), px(50), fix32{}).x == px(40), "a body on the z0 line is stopped");
    check(moved(px(30), px(10) - raw(1), px(50), fix32{}).x == px(80), "one raw outside z0 walks past");
    check(moved(px(30), px(30), px(50), fix32{}).x == px(40), "the z1 line is the wall too");
    check(moved(px(30), px(30) + raw(1), px(50), fix32{}).x == px(80), "one raw outside z1 walks past");
    check(moved(px(50), px(10), px(5), fix32{}).x == px(55), "a body stopped on the face slides along it");
    check(moved(px(40), px(20), fix32{}, px(5)).z == px(25), "and along the side face in z");
}

void test_a_wall_on_the_band_edge_is_sealed() {
    DepthFloor f;
    f.band = FloorRect{px(0), px(0), px(100), px(50)};
    f.add_wall(FloorRect{px(40), px(30), px(60), px(50)});
    DepthBody b{px(30), px(50), fix32{}, px(50), fix32{}, fix32{}};
    slide(f, b);
    check(b.x == px(40), "a body clamped to the band edge does not walk along it through the wall");
}

void test_the_seam_of_two_walls_is_sealed() {
    DepthFloor f;
    f.band = FloorRect{px(0), px(0), px(100), px(100)};
    f.add_wall(FloorRect{px(40), px(10), px(60), px(30)});
    f.add_wall(FloorRect{px(50), px(30), px(70), px(50)});
    DepthBody b{px(30), px(30), fix32{}, px(50), fix32{}, fix32{}};
    slide(f, b);
    check(b.x == px(40), "the line two walls share does not leak");
}

void test_a_corner_is_not_cut() {
    const DepthBody b = moved(px(39), px(9), px(2), px(2));
    check(!(px(40) < b.x && b.x < px(60) && px(10) < b.z && b.z < px(30)),
          "a diagonal step at the corner never ends inside the wall");
    check(b.x == px(41) && b.z == px(10), "x goes first, then z stops on the face");
    check(b.vz == fix32{} && b.vx == px(2), "only the cut axis loses its speed");
}

void test_the_band_clamps_both_ends() {
    check(moved(raw(1), px(5), raw(-2), fix32{}).x == px(0), "x0 of the band");
    check(moved(px(100) - raw(1), px(5), raw(2), fix32{}).x == px(100), "x1 of the band");
    check(moved(px(5), raw(1), fix32{}, raw(-2)).z == px(0), "z0 of the band");
    check(moved(px(5), px(50) - raw(1), fix32{}, raw(2)).z == px(50), "z1 of the band");
    check(moved(px(5), px(50), fix32{}, fix32{}).z == px(50), "the band edge itself is walkable");
}

void test_walls_are_capped() {
    DepthFloor f;
    uint32_t added = 0;
    for (uint32_t i = 0; i <= MAX_WALLS; ++i) added += f.add_wall(FloorRect{}) ? 1u : 0u;
    check(added == MAX_WALLS && f.wall_count == MAX_WALLS, "walls past the cap are refused");
    DepthFloor g;
    check(!g.add_wall(FloorRect{px(10), px(0), px(5), px(5)}), "a wall with x1 < x0 is refused");
    check(!g.add_wall(FloorRect{px(0), px(10), px(5), px(5)}), "a wall with z1 < z0 is refused");
    check(g.add_wall(FloorRect{px(5), px(5), px(5), px(5)}), "a zero-size wall is a post, not an error");
}

} // namespace

int main() {
    std::printf("brawl: the floor plane, walls and the band\n");
    test_the_wall_stops_on_its_edge_from_both_sides();
    test_the_edge_line_is_part_of_the_wall();
    test_a_wall_on_the_band_edge_is_sealed();
    test_the_seam_of_two_walls_is_sealed();
    test_a_corner_is_not_cut();
    test_the_band_clamps_both_ends();
    test_walls_are_capped();
    return test::verdict("framework-brawl-floor");
}
