#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_conveyor_fixture.hpp"
#include "grid.hpp"

// Конвейер (спека #25, В8б) в platform: перенос тем же атомарным правилом, что у опоры, края
// прямоугольника по линии ног, упор в стену, склон и кого лента везёт.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
namespace tilemap = framework::tilemap;

void enter(Ride& r) {
    r.step(1);
    check(r.w.platform_mode(r.id), "the rider does not stand in platform");
}

fix32 platform_velocity(const Ride& r) {
    const SectionBody* s = r.w.s.sections.find(r.id);
    return s != nullptr ? s->c.velocity.x : at(-1);
}

void platform_carries() {
    Ride right(500, FLOOR);
    enter(right);
    right.w.s.level.conveyors.push_back(lane(480, 232, 520, 248, 2));
    check(about(right.moved(1), 2), "the belt does not move a standing fighter in platform");
    right.step(40);
    check(about(right.x(), 520) && platform_velocity(right) == fix32{},
          "the belt carries past its right edge in platform or into the controller velocity");
    Ride left(500, FLOOR);
    enter(left);
    left.w.s.level.conveyors.push_back(lane(480, 232, 520, 248, -2));
    left.step(40);
    check(about(left.x(), 478), "the belt does not carry left off its left edge in platform");
    const auto moves = [](fix32 y0, fix32 y1) {
        Ride r(500, FLOOR);
        enter(r);
        r.w.s.level.conveyors.push_back(lane(at(480), y0, at(520), y1, 2));
        return about(r.moved(1), 2);
    };
    Ride probe(500, FLOOR);
    enter(probe);
    const fix32 z = probe.body().pos.z;
    const fix32 past = fix32::from_raw(z.raw + 1);
    check(moves(z, z + at(8)) && !moves(past, z + at(8)), "the top edge of a belt in platform is not y0");
    check(moves(z - at(8), past) && !moves(z - at(8), z), "the bottom edge of a belt in platform is not y1");
}

void platform_wall() {
    Ride right(500, FLOOR, {{560, 200, 576, 240}});
    enter(right);
    right.w.s.level.conveyors.push_back(lane(480, 232, 640, 248, 3));
    right.step(40);
    check(!(at(552) < right.x()) && at(549) < right.x(), "the belt does not stop a fighter at a wall in platform");
    Ride left(500, FLOOR, {{424, 200, 440, 240}});
    enter(left);
    left.w.s.level.conveyors.push_back(lane(400, 232, 520, 248, -3));
    left.step(40);
    check(!(left.x() < at(448)) && left.x() < at(451), "the belt does not stop a fighter at a left wall in platform");
}

void platform_riders() {
    Ride jumper(500, FLOOR);
    enter(jumper);
    jumper.w.s.level.conveyors.push_back(lane(480, 232, 600, 248, 2));
    jumper.w.step({go(0, true)});
    const fix32 lift_off = jumper.x();
    jumper.step(6);
    check(fix32{} < jumper.body().pos.y && jumper.x() == lift_off, "the belt carries a fighter in the air in platform");
    Ride held(500, FLOOR);
    enter(held);
    held.w.s.level.conveyors.push_back(lane(480, 232, 600, 248, 2));
    held.body().grip.by = EntId{99};
    check(held.moved(1) == fix32{}, "the belt carries a held fighter in platform");
    Ride enemy(500, FLOOR);
    enemy.body().team = 1;
    enter(enemy);
    enemy.w.s.level.conveyors.push_back(lane(480, 232, 520, 248, 2));
    check(about(enemy.moved(1), 2), "the belt does not carry an enemy in platform");
    Ride lying(500, FLOOR);
    enter(lying);
    lying.w.s.level.conveyors.push_back(lane(480, 232, 520, 248, 2));
    lying.body().react = Reaction::Down;
    lying.body().react_ticks = 100;
    check(about(lying.moved(1), 2), "the belt does not carry a lying fighter in platform");
    lying.step(40);
    check(about(lying.x(), 520), "a lying fighter on the belt in platform gathers its travel");
    Ride both(500, FLOOR);
    enter(both);
    both.w.s.level.conveyors.push_back(lane(480, 232, 520, 248, 2));
    both.w.s.level.conveyors.push_back(lane(480, 232, 520, 248, 3));
    check(about(both.moved(1), 2), "overlapping belts in platform do not take the first one");
}

// Пандус 45 градусов из тайлов-склонов от x = 520 до плато на высоте 208 с x = 552. Перенос лентой
// горизонтален и атомарен, как у опоры: вверх по склону точка назначения входит в склон, и боец
// стоит у подножия; вниз он повисает над склоном, и шаг контроллера доводит его до поверхности.
void ramp(Ride& r) {
    for (uint32_t k = 0; k < 8; ++k) {
        r.w.grid.set(130 + k, 19 - k, tilemap::TILE_SOLID | tilemap::TILE_SLOPE);
        r.w.grid.fill(130 + k, 20 - k, 131 + k, 20, tilemap::TILE_SOLID);
    }
}

void platform_slope() {
    const std::vector<Solid> plateau = {{552, 208, 640, 240}};
    Ride up(500, FLOOR, plateau);
    ramp(up);
    enter(up);
    up.w.s.level.conveyors.push_back(lane(480, 200, 600, 248, 2));
    up.step(40);
    const fix32 foot = up.x();
    check(at(505) < foot && foot < at(513) && up.moved(10) == fix32{},
          "the belt in platform carries a fighter up a slope or not to its foot");
    Ride down(570, 208, plateau);
    ramp(down);
    enter(down);
    down.w.s.level.conveyors.push_back(lane(480, 200, 600, 248, -2));
    down.step(80);
    check(down.x() < at(480) && at(478) < down.x() + at(1) && about(down.body().pos.z, FLOOR),
          "the belt in platform does not carry a fighter down a slope off its left edge");
}

} // namespace

int main() {
    platform_carries();
    platform_wall();
    platform_riders();
    platform_slope();
    return verdict("framework-brawl-conveyor-platform");
}
