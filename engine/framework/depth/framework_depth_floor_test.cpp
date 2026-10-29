#include "framework_depth_floor_fixture.hpp"

#include "platform_args.hpp"

// Спека #29 В2б: пол belt — ядро SDK. Случаи стен и краёв обязаны отбить свои сломанные копии: ход
// без остановки на грани проходит стену насквозь, стена с открытыми краями пропускает тело по линии
// своего края, а полоса без клампа выпускает его за край улицы.
namespace {

using framework::depth::DepthBody;
using framework::depth::DepthFloor;
using framework::depth::FloorRect;
using framework::depth::FloorView;
using namespace framework::depth::test;

const Slider SLIDE = framework::depth::slide;
const Blocker BLOCKED = framework::depth::blocked;

void the_wall_stops_on_its_edge_from_both_sides(Slider s, Expect& e) {
    const DepthBody cut = moved(s, px(40) - raw(1), px(20), raw(2), fix32{});
    e(cut.x == px(40), "from the left one raw short of the edge the step stops on x0");
    e(cut.vx == fix32{}, "the cut x step loses its speed");
    e(moved(s, px(40) - raw(1), px(20), raw(1), fix32{}).x == px(40), "a step that ends exactly on x0 is not cut");
    e(moved(s, px(60) + raw(1), px(20), raw(-2), fix32{}).x == px(60), "from the right the step stops on x1");
    e(moved(s, px(50), px(10) - raw(1), fix32{}, raw(2)).z == px(10), "from below on z0");
    e(moved(s, px(50), px(30) + raw(1), fix32{}, raw(-2)).z == px(30), "from above on z1");
    e(moved(s, px(30), px(20), px(50), fix32{}).x == px(40), "a long step does not tunnel");
    e(moved(s, px(40), px(20), raw(1), fix32{}).x == px(40), "standing on x0 the wall still holds");
    e(moved(s, px(40), px(20), raw(-1), fix32{}).x == px(40) - raw(1), "and lets him back off");
}

void the_edge_line_is_part_of_the_wall(Slider s, Expect& e) {
    e(moved(s, px(30), px(10), px(50), fix32{}).x == px(40), "a body on the z0 line is stopped");
    e(moved(s, px(30), px(10) - raw(1), px(50), fix32{}).x == px(80), "one raw outside z0 walks past");
    e(moved(s, px(30), px(30), px(50), fix32{}).x == px(40), "the z1 line is the wall too");
    e(moved(s, px(30), px(30) + raw(1), px(50), fix32{}).x == px(80), "one raw outside z1 walks past");
    e(moved(s, px(50), px(10), px(5), fix32{}).x == px(55), "a body stopped on the face slides along it");
    e(moved(s, px(40), px(20), fix32{}, px(5)).z == px(25), "and along the side face in z");
}

void the_seams_are_sealed(Slider s, Expect& e) {
    DepthFloor<> edge;
    edge.band = FloorRect{px(0), px(0), px(100), px(50)};
    edge.add_wall(FloorRect{px(40), px(30), px(60), px(50)});
    DepthBody b{px(30), px(50), fix32{}, px(50), fix32{}, fix32{}};
    s(edge.view(), b);
    e(b.x == px(40), "a body clamped to the band edge does not walk along it through the wall");
    DepthFloor<> seam;
    seam.band = FloorRect{px(0), px(0), px(100), px(100)};
    seam.add_wall(FloorRect{px(40), px(10), px(60), px(30)});
    seam.add_wall(FloorRect{px(50), px(30), px(70), px(50)});
    DepthBody c{px(30), px(30), fix32{}, px(50), fix32{}, fix32{}};
    s(seam.view(), c);
    e(c.x == px(40), "the line two walls share does not leak");
}

void the_corner_is_not_cut(Slider s, Expect& e) {
    const DepthBody b = moved(s, px(39), px(9), px(2), px(2));
    e(!(px(40) < b.x && b.x < px(60) && px(10) < b.z && b.z < px(30)),
      "a diagonal step at the corner never ends inside the wall");
    e(b.x == px(41) && b.z == px(10), "x goes first, then z stops on the face");
    e(b.vz == fix32{} && b.vx == px(2), "only the cut axis loses its speed");
}

void the_band_clamps_both_ends(Slider s, Expect& e) {
    e(moved(s, raw(1), px(5), raw(-2), fix32{}).x == px(0), "x0 of the band");
    e(moved(s, px(100) - raw(1), px(5), raw(2), fix32{}).x == px(100), "x1 of the band");
    e(moved(s, px(5), raw(1), fix32{}, raw(-2)).z == px(0), "z0 of the band");
    e(moved(s, px(5), px(50) - raw(1), fix32{}, raw(2)).z == px(50), "z1 of the band");
    e(moved(s, px(5), px(50), fix32{}, fix32{}).z == px(50), "the band edge itself is walkable");
}

// Тело сходит с platform и на самый край улицы, поэтому края полосы проходимы; края стены замкнуты, как у
// `slide`, иначе тело встало бы на её линию.
void blocked_closes_the_wall_and_opens_the_band(Blocker is, Expect& e) {
    const DepthFloor<> f = walled();
    e(is(f.view(), px(50), px(20)), "inside the wall is blocked");
    e(is(f.view(), px(40), px(20)), "the x0 line of the wall is blocked");
    e(is(f.view(), px(50), px(10)), "the z0 line of the wall is blocked");
    e(!is(f.view(), px(40) - raw(1), px(20)), "one raw outside the wall is open");
    e(!is(f.view(), px(0), px(0)) && !is(f.view(), px(100), px(50)), "the band corners are open");
    e(is(f.view(), raw(-1), px(20)), "one raw left of the band is blocked");
    e(is(f.view(), px(20), px(50) + raw(1)), "one raw past z1 of the band is blocked");
}

// Ёмкость — параметр хранения, а не зашитая константа: отказ сверх неё идёт и на трёх стенах, и на
// дефолтных шестнадцати.
template <class Floor>
void test_walls_are_capped(uint32_t n) {
    Floor f{};
    uint32_t added = 0;
    for (uint32_t i = 0; i <= n; ++i) added += f.add_wall(FloorRect{}) ? 1u : 0u;
    check(added == n && f.wall_count == n, "walls past the cap are refused");
    check(f.view().walls.size() == n, "the view sees every wall and no more");
    Floor g{};
    check(!g.add_wall(FloorRect{px(10), px(0), px(5), px(5)}), "a wall with x1 < x0 is refused");
    check(!g.add_wall(FloorRect{px(0), px(10), px(5), px(5)}), "a wall with z1 < z0 is refused");
    check(g.add_wall(FloorRect{px(5), px(5), px(5), px(5)}), "a zero-size wall is a post, not an error");
}

void test_the_floor_overloads() {
    const DepthFloor<> f = walled();
    DepthBody b{px(30), px(20), fix32{}, px(50), fix32{}, fix32{}};
    slide(f, b);
    check(b.x == px(40), "slide on the floor stops on the wall face");
    check(blocked(f, px(50), px(20)) && !blocked(f, px(20), px(20)), "blocked on the floor sees the wall");
}

void test_slide() {
    honest(the_wall_stops_on_its_edge_from_both_sides, SLIDE);
    control(the_wall_stops_on_its_edge_from_both_sides, Slider{broken_slide<Break::Tunnel>},
            "control: a step that never stops on the face holds the wall");
    honest(the_edge_line_is_part_of_the_wall, SLIDE);
    control(the_edge_line_is_part_of_the_wall, Slider{broken_slide<Break::OpenEdge>},
            "control: a wall with open edges holds its edge line");
    honest(the_seams_are_sealed, SLIDE);
    control(the_seams_are_sealed, Slider{broken_slide<Break::OpenEdge>},
            "control: open edges hold the band edge and the seam");
    honest(the_corner_is_not_cut, SLIDE);
    control(the_corner_is_not_cut, Slider{broken_slide<Break::Tunnel>},
            "control: a step that never stops on the face holds the corner");
    honest(the_band_clamps_both_ends, SLIDE);
    control(the_band_clamps_both_ends, Slider{broken_slide<Break::NoBand>},
            "control: a band without the clamp holds its ends");
}

void test_blocked() {
    honest(blocked_closes_the_wall_and_opens_the_band, BLOCKED);
    control(blocked_closes_the_wall_and_opens_the_band, Blocker{broken_blocked<Break::OpenEdge>},
            "control: blocked with open edges holds");
    control(blocked_closes_the_wall_and_opens_the_band, Blocker{broken_blocked<Break::NoBand>},
            "control: blocked without the band holds");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework depth: the floor plane, walls and the band\n");
    test_slide();
    test_blocked();
    test_the_floor_overloads();
    test_walls_are_capped<DepthFloor<3>>(3);
    test_walls_are_capped<DepthFloor<>>(16);
    std::printf("framework-depth-floor: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
