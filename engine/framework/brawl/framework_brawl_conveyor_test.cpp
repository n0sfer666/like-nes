#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_conveyor_fixture.hpp"

// Конвейер (спека #25, В8б) в belt: смещение тика у стоящего, края прямоугольника с обеих сторон,
// упор в стену, отсутствие накопления, кого лента везёт и кого нет. Platform — свой тест.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;

bool belt_moves(fix32 x, fix32 z) {
    Ride r(100, 250);
    r.w.s.level.conveyors.push_back(lane(80, 240, 200, 260, 2));
    r.body().pos.x = x;
    r.body().pos.z = z;
    return !(r.moved(1) == fix32{});
}

void belt_carries() {
    Ride right(100, 250);
    right.w.s.level.conveyors.push_back(lane(80, 240, 200, 260, 2));
    check(right.moved(1) == at(2), "the belt does not move a standing body by its speed");
    right.step(80);
    check(right.x() == at(200) && right.body().pos.vx == fix32{}, "the belt carries past its right edge or into vx");
    Ride left(150, 250);
    left.w.s.level.conveyors.push_back(lane(80, 240, 200, 260, -2));
    left.step(80);
    check(left.x() == at(78), "the belt does not carry left off its left edge");
    check(belt_moves(at(80), at(250)) && !belt_moves(before(80), at(250)), "the left edge of a belt is not x0");
    check(belt_moves(before(200), at(250)) && !belt_moves(at(200), at(250)), "the right edge of a belt is not x1");
    check(belt_moves(at(100), at(240)) && !belt_moves(at(100), before(240)), "the top edge of a belt is not y0");
    check(belt_moves(at(100), before(260)) && !belt_moves(at(100), at(260)), "the bottom edge of a belt is not y1");
}

void belt_wall() {
    Ride right(100, 250);
    right.w.s.arena.floor.add_wall(FloorRect{at(150), at(200), at(160), at(300)});
    right.w.s.level.conveyors.push_back(lane(80, 200, 300, 300, 3));
    right.step(40);
    check(right.x() == at(150), "the belt does not stop a body at a wall on its right");
    Ride left(250, 250);
    left.w.s.arena.floor.add_wall(FloorRect{at(150), at(200), at(160), at(300)});
    left.w.s.level.conveyors.push_back(lane(80, 200, 300, 300, -3));
    left.step(40);
    check(left.x() == at(160), "the belt does not stop a body at a wall on its left");
}

void belt_riders() {
    const auto ride = [](void (*set)(Body&)) {
        Ride r(100, 250);
        r.w.s.level.conveyors.push_back(lane(80, 240, 200, 260, 2));
        set(r.body());
        return r.moved(1);
    };
    check(ride([](Body& b) { b.team = 1; }) == at(2), "the belt does not carry an enemy");
    check(ride([](Body& b) {
              b.react = Reaction::Down;
              b.react_ticks = 100;
          }) == at(2),
          "the belt does not carry a lying body");
    check(ride([](Body& b) { b.pos.y = at(10); }) == fix32{}, "the belt carries a body in the air");
    check(ride([](Body& b) { b.hitstop = 5; }) == fix32{}, "the belt carries a body in hitstop");
    check(ride([](Body& b) { b.grip.by = EntId{99}; }) == fix32{}, "the belt carries a held body");
    Ride lying(100, 250);
    lying.w.s.level.conveyors.push_back(lane(80, 240, 200, 260, 2));
    lying.body().react = Reaction::Down;
    lying.body().react_ticks = 100;
    lying.step(80);
    check(lying.x() == at(200), "a lying body coasting on the belt gathers its travel");
    Ride both(100, 250);
    both.w.s.level.conveyors.push_back(lane(80, 240, 200, 260, 2));
    both.w.s.level.conveyors.push_back(lane(80, 240, 200, 260, 3));
    check(both.moved(1) == at(2), "overlapping belts do not take the first one");
}

} // namespace

int main() {
    belt_carries();
    belt_wall();
    belt_riders();
    return verdict("framework-brawl-conveyor");
}
