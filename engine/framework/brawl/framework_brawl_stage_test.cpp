#include <cinttypes>
#include <vector>

#include "body_hash.hpp"
#include "camera.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_stage_fixture.hpp"
#include "hash_mix.hpp"
#include "seat_hash.hpp"
#include "stage_camera_hash.hpp"
#include "viewport_fit.hpp"

namespace {

using namespace framework::brawl;
using framework::physics::mix_u64;
using test::area;
using test::at;
using test::check;
using test::dummy;
using test::strike;
using test::Stage;
using test::walk;

constexpr uint64_t GOLDEN = 0x58d1f6c28a034cbb;
// Края попадания jab места 1: на NEAR_X его достаёт только позиция после поводка этого тика, на
// FAR_X — только позиция до него. Порядок «удары до камеры» меняет исход на обоих.
constexpr int32_t NEAR_X = 418;
constexpr int32_t FAR_X = 446;

uint64_t bodies_hash(const Stage& s) {
    uint64_t h = state_hash(s.pool);
    mix_u64(h, seats_hash(s.seats));
    return h;
}

uint64_t world_hash(const Stage& s) {
    uint64_t h = bodies_hash(s);
    mix_u64(h, camera_hash(s.camera));
    return h;
}

std::vector<Command> apart(const Stage& s) {
    std::vector<Command> c(s.pool.count);
    c[0].input = walk(-1);
    c[1].input = walk(1);
    return c;
}

// Место 1 бьёт на краю зоны, пока место 0 уходит влево: край зоны едет на единицу за тик, и удар
// попадает, только если судит уже зажатую в ЭТОМ тике позицию.
uint32_t order_hits(bool leash_first, int32_t enemy_x) {
    Stage s;
    s.seat(0, 36);
    s.seat(1, 420);
    s.pool.spawn(dummy(enemy_x, 240, -1, 1, s.arena.kinds[0]));
    const BrawlWorld w = s.world();
    HitEvents events;
    uint32_t hits = 0;
    for (uint32_t t = 0; t < 4; ++t) {
        std::vector<Command> c(s.pool.count);
        c[0].input = walk(-1);
        if (t == 0) c[1] = strike(s.arena, s.arena.jab);
        if (leash_first) {
            step_stage(s.pool, c, w, s.camera, s.seats, s.sections, events);
        } else {
            step_brawl(s.pool, c, w, events);
            step_camera(s.camera, s.pool, s.seats, s.sections, w);
        }
        hits += events.count;
    }
    return hits;
}

// Сломанная фикстура «тряска в хеше» — `leak`: центр отрисовки с тряской пишется обратно в сим в
// середине прогона. `bounds` выше вида, поэтому утёкший `y` переживает утечку до конца прогона, а `x`
// следующий тик пересчитывает по игрокам.
uint64_t shaken(bool shake, bool leak) {
    Stage s;
    s.level.bounds = area(0, 0, 1024, 480);
    s.camera.center.y = at(200);
    s.seat(0, 300);
    s.seat(1, 340);
    framework::graphics::Camera drawn;
    framework::graphics::CameraConfig cfg;
    cfg.half_view = framework::graphics::view_zone_half();
    const BrawlWorld w = s.world();
    HitEvents events;
    for (uint64_t t = 0; t < 60; ++t) {
        step_stage(s.pool, apart(s), w, s.camera, s.seats, s.sections, events);
        drawn.center = s.camera.center;
        if (shake && t == 20) framework::graphics::camera_shake(drawn, 10, at(6), static_cast<uint32_t>(t));
        const framework::Vec2 seen = framework::graphics::camera_layer_center(drawn, cfg, t, at(1));
        if (leak && t > 20 && t < 30) s.camera.center = seen;
    }
    return world_hash(s);
}

bool shake_free(bool leak) { return shaken(false, leak) == shaken(true, leak); }

void locked_arena(Stage& s) {
    s.level.bounds = area(0, 0, 640, 216);
    s.level.arenas.push_back({area(100, 0, 400, 216), 1});
    s.camera = StageCamera{{at(200), at(108)}, 1, fix32{}};
    s.seat(0, 150);
    s.seat(1, 250);
}

void run(Stage& s, uint32_t ticks) {
    const BrawlWorld w = s.world();
    HitEvents events;
    for (uint32_t t = 0; t < ticks; ++t) step_stage(s.pool, apart(s), w, s.camera, s.seats, s.sections, events);
}

// Ресим из снапшота «пул + места + камера». Сломанная фикстура — `forget_camera`: камера не в снимке.
// Судят тела без `camera_hash`: запертая арена — состояние, которое из тел не восстановить, и только
// она держит место 0 поводком на 58, а не на 8.
bool resim_matches(bool forget_camera) {
    Stage s;
    locked_arena(s);
    run(s, 20);
    const BodyPool pool = s.pool;
    const Seats seats = s.seats;
    const StageCamera camera = s.camera;
    run(s, 40);
    const uint64_t straight = bodies_hash(s);
    s.pool = pool;
    s.seats = seats;
    s.camera = forget_camera ? StageCamera{} : camera;
    run(s, 40);
    return bodies_hash(s) == straight;
}

// Арена уже вида держит камеру в своей середине до тика 50, игроки расходятся до поводка, на тике 100 место 0 падает, и
// камера уходит за местом 1 до правого края `bounds`, где его держит уже поводок.
uint64_t golden_run(Stage& s) {
    locked_arena(s);
    const BrawlWorld w = s.world();
    HitEvents events;
    uint64_t h = framework::physics::FNV_OFFSET;
    for (uint32_t t = 0; t < 240; ++t) {
        if (t == 50) s.camera.arena = NO_ARENA;
        if (t == 100) s.player(0).hp = 0;
        step_stage(s.pool, apart(s), w, s.camera, s.seats, s.sections, events);
        if (t == 49)
            check(s.camera.center.x == at(250) && s.player(0).pos.x == at(58), "the locked arena loses the leash");
        if (t == 99)
            check(s.player(0).pos.x == at(12) && s.player(1).pos.x == at(396), "the leash does not hold both players");
        mix_u64(h, world_hash(s));
    }
    return h;
}

} // namespace

int main() {
    check(order_hits(true, NEAR_X) == 1, "the hit misses the position the leash set this tick");
    check(order_hits(false, NEAR_X) == 0, "the near probe hits without the leash first");
    check(order_hits(true, FAR_X) == 0, "the hit lands from the position before the leash");
    check(order_hits(false, FAR_X) == 1, "the far probe misses without the leash first");
    check(shake_free(false), "the shake moves the simulation");
    check(!shake_free(true), "the shake leak passes the judge");
    check(resim_matches(false), "restoring pool, seats and camera does not replay the run");
    check(!resim_matches(true), "control: the camera outside the snapshot passes the resim");
    Stage s;
    const uint64_t h = golden_run(s);
    check(s.player(1).pos.x == at(640) && s.camera.center.x == at(448), "the run does not end on the bounds edge");
    if (h != GOLDEN) {
        std::printf("  FAIL: stage golden 0x%016" PRIx64 " != 0x%016" PRIx64 "\n", h, GOLDEN);
        ++test::fails;
    }
    return test::verdict("framework-brawl-stage");
}
