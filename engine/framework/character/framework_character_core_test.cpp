#include "collision.hpp"
#include "push.hpp"
#include "slide.hpp"
#include "support.hpp"

#include <cstdio>

namespace {

using namespace framework;
using namespace framework::character;
int failures = 0;
fix32 px(int32_t v) { return fix32::from_int(v); }
void check(bool ok, const char* why) {
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", why); }
}

struct Floor {
    physics::World world{2};
    CollisionScene scene{&world};
    CharacterHull hull{physics::box(px(4), px(8))};
    physics::BodyId id;
    Floor() {
        physics::BodyDesc d;
        d.key = 1;
        d.type = physics::BodyType::Static;
        d.shape = physics::box(px(100), px(4));
        d.position = {fix32{}, px(104)};
        id = world.add(d);
    }
};

using Slide = SlideResult (*)(const CollisionScene&, const CharacterHull&, Vec2, fix32, Vec2&, Vec2&);

bool floor_course(Slide slide) {
    Floor f;
    SceneHit hit;
    if (!cast_nearest(f.scene, f.hull, {fix32{}, px(80)}, {fix32{}, px(40)}, hit) || hit.body.index != f.id.index) return false;
    Vec2 position{fix32{}, px(80)}, velocity{px(40), px(40)};
    slide(f.scene, f.hull, {px(40), px(40)}, px(1), position, velocity);
    GroundInfo ground;
    return px(90) < position.y && position.y < px(92) && position.x == px(40) && velocity.y == fix32{} &&
           probe_ground(f.scene, f.hull, position, px(1), &ground) && ground.body.index == f.id.index && !ground.oneway;
}

SlideResult through_floor(const CollisionScene&, const CharacterHull&, Vec2 travel, fix32, Vec2& at, Vec2&) {
    at = at + travel;
    return {};
}

using FloorRule = bool (*)(Vec2, fix32);

bool slopes(FloorRule floor) {
    return floor({fix32{}, -px(1)}, fix32{}) && !floor({px(1), fix32{}}, px(100)) &&
           floor({px(1), -px(1)}, px(1)) && !floor({px(1), -px(1)}, fix32{}) &&
           floor({px(2), -px(1)}, px(100)) && !floor({px(2) + fix32::from_raw(1), -px(1)}, px(100)) &&
           !floor({fix32{}, px(1)}, px(100));
}

bool unclamped_floor(Vec2 normal, fix32 slope) {
    return normal.y.raw < 0 && !(slope * abs_fix(normal.y) < abs_fix(normal.x));
}

bool carry(bool real) {
    Floor f;
    physics::BodyDesc d;
    d.key = 2;
    d.type = physics::BodyType::Kinematic;
    d.shape = physics::box(px(8), px(4));
    d.position = {px(200), px(200)};
    const physics::BodyId id = f.world.add(d);
    f.world.mutate(id).velocity = {px(60), fix32{}};
    Vec2 at;
    const fix32 dt = px(1) / px(60);
    if (real && !carry_by_support(f.scene, f.hull, id, dt, at)) return false;
    return at.x == px(60) * dt && at.y == fix32{} &&
           support_velocity(f.scene, f.id).x == fix32{} &&
           support_velocity(f.scene, physics::BodyId{999}).x == fix32{};
}

bool pushed(bool real) {
    physics::World world(1);
    physics::BodyDesc d;
    d.key = 1;
    d.type = physics::BodyType::Kinematic;
    d.shape = physics::box(px(10), px(10));
    const physics::BodyId id = world.add(d);
    world.mutate(id).velocity = {px(60), fix32{}};
    const CollisionScene scene{&world};
    const CharacterHull hull{physics::box(px(4), px(8))};
    Vec2 at{px(13), fix32{}};
    if (real) moved_by_world(scene, hull, {}, false, px(1) / px(60), at);
    return px(13) < at.x && at.y == fix32{};
}

} // namespace

int main() {
    check(floor_course(move_and_slide) && !floor_course(through_floor),
          "collision, slide and ground probe stop at the floor; the no-collision control falls through");
    check(slopes(is_floor) && !slopes(unclamped_floor), "slope predicates clamp at the core limit, including one raw beyond it");
    check(is_ceiling({px(1), px(1)}, px(1)) && !is_ceiling({px(1), -px(1)}, px(1)), "ceiling reverses the floor normal");
    check(carry(true) && !carry(false), "support carries by velocity times dt; static and invalid supports stay still");
    check(pushed(true) && !pushed(false), "a kinematic side pushes an overlapping hull; ignoring it leaves the hull behind");
    std::printf("framework-character-core: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
