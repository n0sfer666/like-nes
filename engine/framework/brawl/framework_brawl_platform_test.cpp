#include <cinttypes>
#include <cstdio>
#include <vector>

#include "body_hash.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_section_fixture.hpp"
#include "hash_mix.hpp"
#include "physics_world.hpp"
#include "seat_hash.hpp"
#include "stage_camera_hash.hpp"
#include "stage_sections_hash.hpp"

// Тела в platform (спека #25, В6г): удары только между телами одного режима, в platform — по
// экранной высоте без проверки глубины; реакция гасит ввод контроллера, отбрасывание переходит в
// его скорость. Голден — прогон через границу секции туда и обратно, с прыжком и ударом.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::physics::mix_u64;

constexpr uint64_t GOLDEN = 0xad9021e2a89108aa;

std::vector<Command> jab_by_first(const Sections& w, uint32_t count) {
    std::vector<Command> c(count);
    c[0] = strike(w.s.arena, w.s.arena.jab);
    return c;
}

uint32_t hits(Sections& w, int32_t attacker_x, int32_t attacker_z, int32_t target_x, int32_t target_z) {
    w.body(attacker_x, attacker_z);
    w.body(target_x, target_z, 1);
    const BrawlWorld world = w.world();
    HitEvents events;
    uint32_t n = 0;
    for (uint32_t t = 0; t < 4; ++t) {
        step_stage(w.s.pool, t == 0 ? jab_by_first(w, 2) : std::vector<Command>(2), world, w.s.camera, w.s.seats,
                   w.s.sections, events);
        n += events.count;
    }
    return n;
}

void by_mode() {
    Sections both;
    check(hits(both, 400, FLOOR, 418, FLOOR) == 1, "a platform body misses a platform body in reach");
    Sections apart;
    check(hits(apart, 392, FLOOR, 406, FLOOR) == 0, "a belt body hits a platform body");
    check(apart.s.sections.count == 1, "the probe target is not in platform");
}

void across_floors() {
    Sections ledge({{408, 228, 428, FLOOR}});
    check(hits(ledge, 418, 228, 438, FLOOR) == 1, "platform hits judge depth instead of the screen height");
    Sections high({{432, 200, 472, FLOOR}});
    check(hits(high, 418, FLOOR, 444, 200) == 0, "a platform hit reaches a body high above on the screen");
    Sections low({{432, 220, 472, FLOOR}});
    check(hits(low, 418, FLOOR, 444, 220) == 1, "a platform hit misses a body whose screen box it crosses");
}

// Цель в platform, уже в реакции, жмёт к бьющему: её путь обязан совпасть с путём без ввода, а
// отбрасывание — увести её от бьющего.
std::vector<fix32> struck_path(bool pressing) {
    Sections w;
    w.body(400);
    const EntId target = w.body(418, FLOOR, 1);
    std::vector<fix32> path;
    for (uint32_t t = 0; t < 12; ++t) {
        std::vector<Command> c = t == 0 ? jab_by_first(w, 2) : std::vector<Command>(2);
        if (pressing && !(w.get(target).react == Reaction::None)) c[1] = go(-1);
        w.step(c);
        if (!(w.get(target).react == Reaction::None)) path.push_back(w.get(target).pos.x);
    }
    return path;
}

void reaction() {
    const std::vector<fix32> pressed = struck_path(true);
    const std::vector<fix32> idle = struck_path(false);
    check(!pressed.empty() && pressed == idle, "the controller takes input while the body reacts");
    check(!idle.empty() && at(418) < idle.back(), "the knockback does not carry into the controller");
}

// Контроллер сам поднимает `crushed`, когда движущееся тело вжимает его в стену: тело brawl обязано
// видеть и его, а не только то, что поднял край автоскролла.
bool shoved_into_wall(bool wall) {
    Sections w(wall ? std::vector<Solid>{{420, GRID_TOP, 432, FLOOR}} : std::vector<Solid>{});
    framework::physics::World world(4);
    world.set_gravity({});
    framework::physics::BodyDesc d;
    d.key = 1;
    d.type = framework::physics::BodyType::Kinematic;
    d.shape = framework::physics::box(at(8), at(8));
    d.position = {at(480), at(224)};
    d.velocity = {at(-240), fix32{}};
    world.add(d);
    w.platform.scene.world = &world;
    const EntId id = w.body(444);
    bool crushed = false;
    for (uint32_t t = 0; t < 12; ++t) {
        world.step(w.platform.dt);
        w.step({});
        crushed = crushed || w.get(id).crushed;
    }
    return w.platform_mode(id) && crushed;
}

void controller_crush() {
    check(shoved_into_wall(true), "the controller crush of a platform body does not reach the body");
    check(!shoved_into_wall(false), "a platform body shoved with room to go is crushed");
}

uint64_t tick_hash(const Sections& w) {
    uint64_t h = state_hash(w.s.pool);
    mix_u64(h, seats_hash(w.s.seats));
    mix_u64(h, camera_hash(w.s.camera));
    mix_u64(h, sections_hash(w.s.sections));
    return h;
}

struct Trace {
    bool in = false;
    bool air_out = false;
    bool early = false;
};

// Место 0 входит в platform шагом, бьёт врага там, прыгает, возвращается в belt после
// приземления; место 1 стоит в belt и держит камеру на середине охвата.
uint64_t golden_run(Sections& w, Trace& trace) {
    w.s.camera.center = {at(355), at(FLOOR - 24)};
    w.s.seat(0, 380);
    w.s.seat(1, 330);
    w.s.pool.spawn(dummy(470, FLOOR, -1, 1, w.s.arena.kinds[0]));
    uint64_t h = framework::physics::FNV_OFFSET;
    bool struck = false;
    for (uint32_t t = 0; t < 240; ++t) {
        std::vector<Command> c(w.s.pool.count);
        if (t < 60 && !struck) c[0] = go(1);
        if (!struck && at(440) < w.s.player(0).pos.x) {
            c[0] = strike(w.s.arena, w.s.arena.jab);
            struck = true;
        }
        if (t >= 60 && t < 140) c[0] = go(-1, t >= 70 && t < 80);
        w.step(c);
        const Body& p = w.s.player(0);
        const bool platform = w.platform_mode(p.id);
        trace.in = trace.in || platform;
        if (p.pos.x < at(PLATFORM_X0) && fix32{} < p.pos.y) trace.air_out = trace.air_out || platform;
        if (trace.in && !platform && fix32{} < p.pos.y) trace.early = true;
        mix_u64(h, tick_hash(w));
    }
    return h;
}

} // namespace

int main() {
    by_mode();
    across_floors();
    reaction();
    controller_crush();
    Sections w;
    Trace trace;
    const uint64_t h = golden_run(w, trace);
    check(trace.in && !w.platform_mode(w.s.player(0).id) && w.s.player(0).pos.x < at(PLATFORM_X0),
          "the golden run does not cross the boundary both ways");
    check(trace.air_out && !trace.early, "the golden run leaves platform in the air");
    check(w.s.pool.bodies[2].hp < 100, "the golden jab misses the enemy in platform");
    if (h != GOLDEN) {
        std::printf("  FAIL: platform golden 0x%016" PRIx64 " != 0x%016" PRIx64 "\n", h, GOLDEN);
        ++fails;
    }
    return verdict("framework-brawl-platform");
}
