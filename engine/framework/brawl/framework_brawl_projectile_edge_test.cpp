#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_shot_fixture.hpp"
#include "section_exit.hpp"

// Гибель снаряда и отказы выпуска (спека #25, В8г): в belt — стена, край полосы, пол; в platform —
// твёрдый тайл (односторонний пропускает); `x` в секции другого режима — гибель в обе стороны.
// Упёршийся снаряд встаёт в точку упора и бьёт в этом тике, вышедший из режима — на ближней из
// пересечённых границ, выпущенный в чужом — на его границе со стороны владельца; запись снаряда места не занимает.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::Vec2;
namespace character = framework::character;
namespace scene = framework::scene;
namespace tilemap = framework::tilemap;

void dies_on_the_floor_bounds() {
    Shots wall;
    wall.sec.s.arena.floor.add_wall(FloorRect{at(200), at(200), at(220), at(300)});
    check(wall.lifetime(wall.shoot(wall.fighter(100, 0), aim(180, 8), 20)) == 3, "a belt shot flies through a wall");
    Shots band;
    check(band.lifetime(band.shoot(band.fighter(100, 0), DepthBody{at(110), at(290), {}, {}, at(6), {}}, 20)) == 2,
          "a belt shot leaves the depth band");
    Shots floor;
    check(floor.lifetime(floor.shoot(floor.fighter(100, 0), DepthBody{at(110), at(LANE), at(4), {}, {}, at(-3)}, 20)) ==
              2,
          "a belt shot sinks under the floor");
    Shots open;
    check(open.lifetime(open.shoot(open.fighter(100, 0), aim(110, 8), 20)) == 20 && open.events.count == 0,
          "control: a belt shot in the open does not live its life");
}

void dies_leaving_its_mode() {
    Shots right;
    check(right.lifetime(right.shoot(right.fighter(380, 0), aim(390, 4), 20)) == 3,
          "a belt shot flies on into a platform section");
    Shots left;
    const EntId back = left.shoot(left.fighter(820, 0), aim(810, -4), 20);
    check(left.lifetime(back) == 3 && left.find(back)->pos.x == at(800),
          "a belt shot flies back into a platform section");
    Shots inside;
    const EntId over = inside.shoot(inside.fighter(396, 0), aim(404, 0), 20);
    check(inside.lifetime(over) == 1 && inside.find(over)->pos.x < at(400),
          "a belt shot spawned over a platform section stays in platform");
    Shots deep;
    const EntId on = deep.shoot(deep.fighter(390, 0), aim(610, 4), 20);
    const EntId under = deep.shoot(deep.fighter(810, 0), aim(590, -4), 20);
    check(deep.find(on)->pos.x < at(400) && at(399) < deep.find(on)->pos.x && deep.find(under)->pos.x == at(800),
          "a belt shot spawned deep in a platform section leaves it away from its owner");
    for (const int32_t dir : {-1, 1}) {
        Shots p;
        const int32_t edge = dir < 0 ? PLATFORM_X0 : PLATFORM_X1;
        const EntId owner = p.fighter(edge - 20 * dir, 0, FLOOR);
        p.step();
        const EntId shot = p.shoot(owner, aim(edge - 10 * dir, 4 * dir, FLOOR), 20);
        check(p.sec.platform_mode(owner) && p.sec.platform_mode(shot), "a platform owner's shot is not in platform");
        check(p.lifetime(shot) == 3 && (p.find(shot)->pos.x < at(edge)) == (dir > 0),
              "a platform shot flies on into a belt section");
        p.step();
        check(p.find(shot) == nullptr && !p.sec.platform_mode(shot), "a spent platform shot keeps its body or entry");
    }
}

void strikes_at_the_mode_edge() {
    Shots w;
    const EntId target = w.fighter(396, 1);
    const EntId shot = w.shoot(w.fighter(300, 0), aim(350, 60), 20);
    w.step();
    check(w.events.count == 1 && w.events.at[0].target == target && !w.flying(shot) && w.find(shot)->pos.x < at(400) &&
                  at(399) < w.find(shot)->pos.x,
          "a belt shot leaving its mode does not strike at the edge");
    Shots slant;
    const EntId aslant =
            slant.shoot(slant.fighter(300, 0), DepthBody{at(350), at(LANE), at(10), at(60), at(6), at(-6)}, 20);
    slant.step();
    const DepthBody& d = slant.find(aslant)->pos;
    check(at(254) < d.z && d.z < at(256) && at(4) < d.y && d.y < at(6),
          "a shot stopped at its mode edge keeps the depth and height of the full step");
}

fix32 leap(std::initializer_list<int32_t> platforms, int32_t owner, int32_t x, int32_t vx) {
    Shots w;
    w.sec.s.level.sections.clear();
    for (const int32_t x0 : platforms)
        w.sec.s.level.sections.push_back({area(x0, 0, x0 + 200, 400), scene::SectionMode::Platform});
    const EntId shot = w.shoot(w.fighter(owner, 0), aim(x, vx), 20);
    w.step();
    return w.flying(shot) ? fix32{} : w.find(shot)->pos.x;
}

void leaps_to_the_nearest_edge() {
    check(leap({1400, 400, 1000}, 300, 350, 1200) == before(400), "a belt shot leaps over the nearest platform section");
    check(leap({100, 900, 500}, 1200, 1150, -1100) == at(1100), "a belt shot leaps back over the nearest platform section");
}

bool leaves_past(bool shot) {
    Shots w;
    const EntId walker = w.fighter(780, 0, FLOOR);
    w.step();
    const EntId owner = w.fighter(900, 0, FLOOR);
    if (shot) w.shoot(owner, aim(806, 0, FLOOR), 30);
    if (!shot) w.fighter(806, 1, FLOOR);
    for (int t = 0; t < 20; ++t) w.step({go(1)});
    return !w.sec.platform_mode(walker);
}

void frees_the_exit() {
    check(!leaves_past(false), "control: a belt fighter at the exit does not hold the walker in platform");
    check(leaves_past(true), "a belt shot at the exit holds the walker in platform");
}

void platform_tiles() {
    Shots solid({{560, 200, 580, 240}});
    const EntId owner = solid.fighter(500, 0, FLOOR);
    solid.step();
    const EntId shot = solid.shoot(owner, aim(510, 4, FLOOR), 30);
    uint32_t t = 0;
    while (solid.flying(shot) && t < 30) {
        check(!(at(560) < solid.find(shot)->pos.x + at(4)), "a platform shot flies into a solid tile");
        solid.step();
        ++t;
    }
    check(10 < t && t < 14 && at(552) < solid.find(shot)->pos.x, "a platform shot does not stop at a solid tile");
    Shots oneway;
    const auto flags = static_cast<tilemap::TileFlags>(tilemap::TILE_SOLID | tilemap::TILE_ONEWAY);
    oneway.sec.grid.fill(cell(560, 0), cell(200, GRID_TOP), cell(580, 0), cell(240, GRID_TOP), flags);
    const EntId by = oneway.fighter(500, 0, FLOOR);
    oneway.step();
    check(oneway.lifetime(oneway.shoot(by, aim(510, 4, FLOOR), 25)) == 25, "a platform shot stops at a one-way tile");
}

void strikes_where_it_stops() {
    Shots wall;
    wall.sec.s.arena.floor.add_wall(FloorRect{at(200), at(200), at(220), at(300)});
    const EntId pinned = wall.fighter(196, 1);
    const EntId shot = wall.shoot(wall.fighter(100, 0), aim(150, 60), 20);
    wall.step();
    check(wall.events.count == 1 && wall.events.at[0].target == pinned && wall.hp(pinned) == 90 &&
              !wall.flying(shot) && !(at(200) < wall.find(shot)->pos.x) && wall.find(shot)->pos.vx == fix32{},
          "a belt shot stopped by a wall does not strike where it stops");
    Shots tile({{560, 200, 580, 240}});
    const EntId owner = tile.fighter(500, 0, FLOOR);
    const EntId near = tile.fighter(548, 1, FLOOR);
    tile.step();
    const EntId flat = tile.shoot(owner, aim(505, 60, FLOOR), 20);
    tile.step();
    check(tile.sec.platform_mode(near) && tile.events.count == 1 && tile.events.at[0].target == near &&
              !tile.flying(flat) && !(at(556) < tile.find(flat)->pos.x),
          "a platform shot stopped by a solid tile does not strike where it stops");
}

bool taken_at_origin(bool shot) {
    Shots w;
    const EntId owner = w.fighter(500, 0, FLOOR);
    w.step();
    if (shot) w.shoot(owner, aim(510, 0, FLOOR), 30);
    if (!shot) w.sec.s.sections.add(w.fighter(0, 1, FLOOR), character::Character{});
    return platform_taken(w.pool(), w.sec.s.sections, EntId{}, Vec2{}, w.world());
}

void shots_take_no_room() {
    check(taken_at_origin(false), "control: a fighter's entry does not take its point");
    check(!taken_at_origin(true), "a shot's entry takes a point in platform");
}

void spawn_refusals() {
    Shots w;
    const EntId owner = w.fighter(100, 0);
    const uint32_t count = w.pool().count;
    check(w.shoot(EntId{}, aim(110, 4), 5) == EntId{} && w.shoot(owner, aim(110, 4), 0) == EntId{} &&
              w.shoot(owner, aim(110, 4), 5, 9) == EntId{} && w.shoot(owner, aim(110, 4), 5, CRATE) == EntId{} &&
              w.shoot(owner, aim(110, 4), 5, FIGHTER) == EntId{},
          "a shot spawns without an owner, a life or a projectile kind");
    w.kinds[CLIP_SHOT].move_count = 0;
    check(w.shoot(owner, aim(110, 4), 5, CLIP_SHOT) == EntId{} && w.pool().count == count,
          "a shot spawns from a kind without a strike or leaves a body behind");
    while (w.pool().count < POOL_CAPACITY) w.fighter(0, 1);
    check(w.shoot(owner, aim(110, 4), 5) == EntId{}, "a shot spawns into a full pool");
    Shots belt;
    const EntId shot = belt.shoot(belt.fighter(100, 0), aim(110, 4), 5);
    check(shot != EntId{} && !belt.sec.platform_mode(shot), "a belt owner's shot lands in platform");
}

} // namespace

int main() {
    dies_on_the_floor_bounds();
    dies_leaving_its_mode();
    strikes_at_the_mode_edge();
    leaps_to_the_nearest_edge();
    platform_tiles();
    frees_the_exit();
    strikes_where_it_stops();
    shots_take_no_room();
    spawn_refusals();
    return verdict("framework-brawl-projectile-edge");
}
