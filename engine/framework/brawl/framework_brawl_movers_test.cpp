#include <cinttypes>
#include <cstdio>

#include "body_hash.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_path_fixture.hpp"
#include "framework_brawl_section_fixture.hpp"
#include "hash_mix.hpp"
#include "stage_movers_hash.hpp"
#include "stage_movers_snapshot.hpp"
#include "stage_sections_hash.hpp"

// Платформы по `path` (спека #25, В8а): лифт, везущий бойца вверх и вбок опорой контроллера, и
// снапшот, в котором забытый мир или забытое следование расходятся с честным пересчётом.
namespace {

constexpr uint64_t GOLDEN = 0x42a1d3b0296fcfc1;

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::Vec2;
namespace scene = framework::scene;

// Лифт выходит из пола: верх на 4 px ниже линии пола, вверх до 48 px над ней, затем вправо,
// маятником. Верх ровно на линии пола был бы касанием, и вход в секцию счёл бы место занятым.
scene::Path lift_path(int32_t right) { return path({{500, 248}, {500, 196}, {right, 196}}, 1, false); }

struct Lift {
    Sections w;
    StageMovers m;
    EntId id;
    explicit Lift(const scene::Path& p = lift_path(580)) : w(), m(level(w, p)), id(w.body(500)) {
        w.platform.scene.world = &m.world;
    }
    static const scene::LevelObjects& level(Sections& w, const scene::Path& p) {
        w.s.level.paths.push_back(p);
        return w.s.level;
    }
    void step(uint32_t ticks) {
        for (uint32_t t = 0; t < ticks; ++t) {
            HitEvents events;
            step_stage(w.s.pool, {}, w.world(), w.s.camera, w.s.seats, w.s.sections, events, &m);
        }
    }
    fix32 feet() const {
        const SectionBody* s = w.s.sections.find(id);
        return s != nullptr ? s->c.position.y + hull_foot(w.platform) : at(-1);
    }
    uint64_t hash() const {
        uint64_t h = state_hash(w.s.pool);
        framework::physics::mix_u64(h, sections_hash(w.s.sections));
        framework::physics::mix_u64(h, movers_hash(m));
        return h;
    }
};

void lift_carries() {
    Lift lift;
    lift.step(1);
    check(lift.w.platform_mode(lift.id), "the rider does not stand in platform");
    lift.step(51);
    check(about(lift.feet(), 192), "the lift does not raise the rider");
    lift.step(80);
    check(about(lift.w.get(lift.id).pos.x, 580) && about(lift.feet(), 192), "the lift does not carry the rider sideways");
    lift.step(80);
    check(about(lift.w.get(lift.id).pos.x, 500), "the lift does not bring the rider back");
    std::printf("  movers lift hash = 0x%016" PRIx64 "\n", lift.hash());
    check(lift.hash() == GOLDEN, "golden of the lift run");
    Lift parked(path({{500, 248}}, 1, false));
    parked.step(132);
    check(about(parked.w.get(parked.id).pos.x, 500) && about(parked.feet(), 240), "a parked lift moves the rider");
}

// Потолок загрузчика, а не середина: подобрав бойца, лифт уходит в воздух — вбок на 280 px, вверх и
// вниз на 46 — по 32 px за тик, с разворотом и остатком хода. Ноги обязаны идти по верху лифта каждый
// тик. Путь не задевает тайлов: перенос атомарен, и на пути сквозь пол боец зависал бы над ним.
void lift_carries_at_the_cap() {
    Lift lift(lift_path(780));
    lift.step(52);
    lift.w.s.level.paths[0].points[0] = {at(500), at(150)};
    lift.w.s.level.paths[0].speed = at(32);
    for (uint32_t t = 0; t < 48; ++t) {
        lift.step(1);
        const Vec2 top = lift.m.world.body(framework::physics::BodyId{0}).position - Vec2{{}, at(4)};
        const bool rides = lift.w.platform_mode(lift.id) && about(lift.w.get(lift.id).pos.x - top.x, 0) &&
                           about(lift.feet() - top.y, 0);
        check(rides, "a lift at the speed cap drops or loses the rider");
        if (!rides) {
            std::printf("    tick %u: rider x %d feet %d, lift top %d,%d\n", t, lift.w.get(lift.id).pos.x.raw,
                        lift.feet().raw, top.x.raw, top.y.raw);
            return;
        }
    }
}

// Лифт из пола на скоростях до порога из scenes-and-levels.md (12 px за тик), два с лишним цикла
// каждая: ноги идут по верху лифта, пока он над полом, и стоят на тайлах, пока он в полу. Перенос
// атомарен, и спуск сквозь линию пола оставляет седока на высоте прошлого тика — он падает сам,
// поэтому зазор вверх терпим до хода за тик и на 3 тика, а вниз, в лифт или в пол, — нет. С 13
// подъём из пола проходит сквозь стоящего бойца: это граница, а не допуск.
void lift_through_the_floor() {
    for (int32_t speed = 1; speed <= 12; ++speed) {
        Lift lift;
        lift.step(1);
        lift.w.s.level.paths[0].speed = at(speed);
        uint32_t hanging = 0;
        for (uint32_t t = 0; t < 600; ++t) {
            lift.step(1);
            const Vec2 top = lift.m.world.body(framework::physics::BodyId{0}).position - Vec2{{}, at(4)};
            const fix32 gap = framework::min_fix(top.y, at(FLOOR)) - lift.feet();
            hanging = at(1) < gap ? hanging + 1 : 0;
            const bool rides = lift.w.platform_mode(lift.id) && !(gap < -at(1)) && !(at(speed) < gap) && hanging <= 3;
            check(rides, "a lift through the floor sinks, drops or leaves the rider");
            if (!rides) {
                std::printf("    speed %d tick %u: feet %d, lift top %d\n", speed, t, lift.feet().raw, top.y.raw);
                return;
            }
        }
    }
}

enum class Forget : uint8_t { Nothing, World, Follow };

uint64_t resim(Forget forget) {
    Lift lift;
    lift.step(30);
    const BodyPool pool = lift.w.s.pool;
    const StageSections sections = lift.w.s.sections;
    const StageCamera camera = lift.w.s.camera;
    MoversSnapshot snap;
    save_movers(lift.m, snap);
    lift.step(70);
    const uint64_t straight = lift.hash();
    lift.w.s.pool = pool;
    lift.w.s.sections = sections;
    lift.w.s.camera = camera;
    if (forget == Forget::Nothing) restore_movers(snap, lift.m);
    if (forget == Forget::World) lift.m.follow = snap.follow;
    if (forget == Forget::Follow) snap.world.apply(lift.m.world);
    lift.step(70);
    return straight ^ lift.hash();
}

void snapshot() {
    check(resim(Forget::Nothing) == 0, "a restored snapshot does not replay the lift");
    check(resim(Forget::World) != 0, "a snapshot without the physics world replays the lift");
    check(resim(Forget::Follow) != 0, "a snapshot without the follow state replays the lift");
    Lift lift;
    const uint64_t before = movers_hash(lift.m);
    lift.m.follow[0].direction = -1;
    const uint64_t turned = movers_hash(lift.m);
    lift.m.follow[0].traveled = at(1);
    const uint64_t moved = movers_hash(lift.m);
    lift.m.follow[0].segment = 1;
    check(!(before == turned) && !(turned == moved) && !(moved == movers_hash(lift.m)),
          "the movers hash does not see the follow state");
}

} // namespace

int main() {
    lift_carries();
    lift_carries_at_the_cap();
    lift_through_the_floor();
    snapshot();
    return verdict("framework-brawl-movers");
}
