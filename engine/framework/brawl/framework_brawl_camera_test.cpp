#include "framework_brawl_check.hpp"
#include "framework_brawl_stage_fixture.hpp"
#include "stage_camera_hash.hpp"

namespace {

using namespace framework::brawl;
using framework::scene::LevelObjects;
using test::area;
using test::at;
using test::check;
using test::Stage;

using CameraStep = void (*)(StageCamera&, BodyPool&, const Seats&, const BrawlWorld&);

// Сломанная фикстура «камера по одному игроку»: второе место для неё не существует.
void follow_first(StageCamera& c, BodyPool& pool, const Seats& seats, const BrawlWorld& w) {
    Seats one = seats;
    one.at[1].present = false;
    step_camera(c, pool, one, w);
}

bool follows_both(CameraStep step) {
    Stage s;
    s.seat(0, 100);
    s.seat(1, 300);
    step(s.camera, s.pool, s.seats, s.world());
    return s.camera.center.x == at(200);
}

fix32 center_x(int32_t x0, int32_t x1, const LevelObjects& level, int16_t arena = NO_ARENA) {
    Stage s;
    s.level = level;
    s.camera.arena = arena;
    s.seat(0, x0);
    s.seat(1, x1);
    s.camera_step();
    return s.camera.center.x;
}

fix32 center_y(int32_t y, const LevelObjects& level) {
    Stage s;
    s.level = level;
    s.camera.center.y = at(y);
    s.seat(0, 300);
    s.camera_step();
    return s.camera.center.y;
}

void center() {
    check(follows_both(step_camera), "the center is not the middle of both live players");
    check(!follows_both(follow_first), "the one-player camera passes the judge");
    Stage s;
    s.camera.center.y = at(120);
    s.seat(0, 300);
    s.seat(1, 100);
    s.camera_step();
    check(s.camera.center.x == at(200) && s.camera.center.y == at(120), "belt: x follows, y stays");
}

void only_live() {
    Stage dead;
    dead.seat(0, 100);
    dead.seat(1, 300).hp = 0;
    dead.camera_step();
    check(dead.camera.center.x == at(100), "a player with no hp pulls the camera");
    Stage absent;
    absent.seat(0, 100);
    absent.seat(1, 300);
    absent.seats.at[1].present = false;
    absent.camera_step();
    check(absent.camera.center.x == at(100), "an absent seat pulls the camera");
    Stage gone;
    gone.seat(0, 300);
    gone.seat(1, 100);
    gone.pool.despawn(gone.seats.at[0].body);
    gone.camera_step();
    check(gone.camera.center.x == at(100), "a seat without a body pulls the camera");
    Stage none;
    none.level.bounds = area(0, 0, 640, 216);
    none.camera.center = {at(5), at(5)};
    none.seat(0, 600).hp = 0;
    none.camera_step();
    check(none.camera.center == framework::Vec2{at(5), at(5)}, "no live players: the camera moved");
}

void clamp() {
    LevelObjects level;
    level.bounds = area(0, 0, 640, 216);
    check(center_x(10, 20, level) == at(192), "bounds: the left edge does not hold");
    check(center_x(630, 620, level) == at(448), "bounds: the right edge does not hold");
    check(center_x(100, 400, level) == at(250), "bounds: the middle moved");
    LevelObjects tall;
    tall.bounds = area(0, 0, 640, 480);
    check(center_y(-50, tall) == at(108), "bounds: the top edge does not hold y");
    check(center_y(600, tall) == at(372), "bounds: the bottom edge does not hold y");
    check(center_y(200, tall) == at(200), "bounds: y moved in the middle");
    level.bounds = area(0, 0, 300, 216);
    check(center_x(10, 20, level) == at(150), "bounds narrower than the view: not the middle");

    LevelObjects arenas;
    arenas.bounds = area(0, 0, 2048, 216);
    arenas.arenas.push_back({area(640, 0, 1200, 216), 2});
    arenas.arenas.push_back({area(1300, 0, 1500, 216), 3});
    arenas.arenas.push_back({area(1600, 0, 1700, 216), NO_ARENA});
    check(center_x(700, 710, arenas, 2) == at(832), "arena: the left edge does not hold");
    check(center_x(1150, 1160, arenas, 2) == at(1008), "arena: the right edge does not hold");
    check(center_x(900, 910, arenas, 2) == at(905), "arena: the middle moved");
    check(center_x(700, 710, arenas) == at(705), "an arena clamps the camera without the lock");
    check(center_x(700, 710, arenas, 3) == at(1400), "arena narrower than the view: not the middle");
    arenas.bounds = area(0, 0, 900, 216);
    check(center_x(1150, 1160, arenas, 2) == at(708), "bounds do not win over the arena");
}

void leash() {
    Stage s;
    s.seat(0, 0).pos.vx = at(-2);
    s.seat(1, 600).pos.vx = at(2);
    s.camera_step();
    check(s.player(0).pos.x == at(108) && s.player(1).pos.x == at(492), "the leash does not hold both edges");
    check(s.player(0).pos.vx == fix32{} && s.player(1).pos.vx == fix32{}, "the leash keeps the outward speed");
    Stage edge;
    edge.seat(0, 0).pos.vx = at(-2);
    edge.seat(1, 384).pos.vx = at(2);
    edge.camera_step();
    check(edge.player(0).pos.x == at(0) && edge.player(1).pos.x == at(384), "the leash moves bodies on its edge");
    check(edge.player(0).pos.vx == at(-2) && edge.player(1).pos.vx == at(2), "the leash stops bodies on its edge");
    Stage locked;
    locked.level.arenas.push_back({area(640, 0, 1024, 216), 1});
    locked.camera.arena = 1;
    locked.seat(0, 600);
    locked.seat(1, 700);
    locked.camera_step();
    check(locked.camera.center.x == at(832) && locked.player(0).pos.x == at(640) && locked.player(1).pos.x == at(700),
          "the leash ignores the clamped center");
}

void leash_floor() {
    Stage wall;
    wall.arena.floor.add_wall(FloorRect{at(2), at(200), at(30), at(300)});
    wall.seat(0, 0);
    wall.seat(1, 450);
    wall.camera_step();
    check(wall.player(0).pos.x == at(2) && wall.player(1).pos.x == at(417), "the leash pushes a body into a wall");
    check(wall.player(0).pos.vx == fix32{}, "the wall keeps the leash speed");
    Stage band;
    band.level.arenas.push_back({area(1200, 0, 1584, 216), 1});
    band.camera.arena = 1;
    band.seat(0, 900);
    band.camera_step();
    check(band.camera.center.x == at(1392) && band.player(0).pos.x == at(1000),
          "the leash pushes a body out of the band");
}

struct Field {
    const char* name;
    void (*bump)(StageCamera&);
};

const Field FIELDS[] = {
    {"center x", [](StageCamera& c) { c.center.x.raw += 1; }},
    {"center y", [](StageCamera& c) { c.center.y.raw += 1; }},
    {"arena", [](StageCamera& c) { c.arena = 1; }},
    {"scroll", [](StageCamera& c) { c.scroll.raw += 1; }},
};

void fields() {
    const StageCamera base{{at(200), at(108)}, NO_ARENA, fix32{}};
    for (const Field& f : FIELDS) {
        StageCamera c = base;
        f.bump(c);
        if (camera_hash(c) == camera_hash(base)) {
            std::printf("  FAIL: camera field %s is outside the hash\n", f.name);
            ++test::fails;
        }
    }
}

} // namespace

int main() {
    center();
    only_live();
    clamp();
    leash();
    leash_floor();
    fields();
    return test::verdict("framework-brawl-camera");
}
