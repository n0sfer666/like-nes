#include <cstdint>
#include <cstdio>
#include <vector>

// Подмена глобальных `operator new`/`delete` — ровно один TU на программу, поэтому она здесь, а не
// в измерительном заголовке (то же основание, что в `framework_physics_perf_test.cpp`).
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "framework_physics_observed.hpp"
#include "framework_physics_snapshot_scene.hpp"
#include "snapshot.hpp"
#include "world.hpp"

// Основание отката вертикали 1 спеки #22: снимок состояния мира и возврат в него. Устройство сцены
// и почему она именно такая — `framework_physics_snapshot_scene.hpp`.
namespace {

using framework::physics::Body;
using framework::physics::BodyId;
using framework::physics::World;
using framework::physics::WorldSnapshot;
namespace scene = framework::physics::snapshot_scene;
using framework::physics::observed::Observed;
using framework::physics::observed::observe;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void run(World& w, uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; ++i) w.step(scene::DT);
}

// Сверка ПОИМЁННАЯ: «миры разошлись» и «разошлась раскладка покоя» — разные сообщения, и второе
// называет поле снимка. Устройство набора — `framework_physics_observed.hpp`.
void check_same(const Observed& got, const Observed& want, const char* where) {
    const char* diff = framework::physics::observed::first_difference(got, want);
    if (diff == nullptr) return;
    std::printf("  FAIL: %s: %s differs\n", where, diff);
    ++fails;
}

// Эталон — непрерывный прогон без единого снятия; с ним сверяется всё остальное в этом файле.
struct Reference {
    Observed before_hit;
    Observed at_hit;
    Observed after_hit;
    Observed end;
};

// Сцена утверждается ПО ДОРОГЕ, а не описывается комментарием: башня, тихо переставшая замирать или
// не дождавшаяся снаряда, унесла бы смысл всех гейтов ниже, оставив их зелёными.
Reference reference() {
    World w{32};
    scene::build(w);
    run(w, scene::SETTLES_BY);
    check(scene::frozen_count(w) == scene::BOXES, "scene: the tower freezes before the take point");
    check(w.recalled_pairs() > 0, "scene: freezing turns the warm-start cache on");

    run(w, scene::TAKE_BEFORE_HIT - scene::SETTLES_BY);
    check(w.trigger_count() == 1, "scene: the shot is inside the trigger zone at the first take");
    const Observed before = observe(w);

    run(w, scene::TAKE_AT_HIT - scene::TAKE_BEFORE_HIT);
    check(scene::frozen_count(w) == 0, "scene: the shot wakes the island");
    check(w.recalled_pairs() == 0, "scene: nothing comes from the cache while the island is awake");
    const Observed at_hit = observe(w);

    run(w, scene::HITS_BY - scene::TAKE_AT_HIT);

    run(w, scene::TAKE_AFTER_HIT - scene::HITS_BY);
    check(scene::frozen_count(w) == scene::BOXES, "scene: the island freezes again after the hit");
    const Observed after = observe(w);

    run(w, scene::RUN_TO - scene::TAKE_AFTER_HIT);
    check(w.trigger_count() == 0, "scene: the shot has left the trigger zone by the end");
    return {before, at_hit, after, observe(w)};
}

// Один круг снятия и возврата. Кругов делается несколько: снимок обязан переживать МНОГОКРАТНЫЙ
// возврат, а реализация, портящая сам снимок ради скорости (swap вместо копии — первое, что тут
// напрашивается), первый круг прошла бы честно и вернула бы мир в пустоту на втором.
void gate_take(uint32_t at, const Observed& at_take, const Observed& at_end) {
    World w{32};
    scene::build(w);
    run(w, at);

    WorldSnapshot snap;
    check(!snap.taken(), "a fresh snapshot holds nothing");
    snap.capture(w);
    check(snap.taken(), "a taken snapshot says so");
    check_same(observe(w), at_take, "the take point against the straight run");

    for (int lap = 0; lap < 3; ++lap) {
        run(w, scene::RUN_TO - at);
        check_same(observe(w), at_end, "replay from the snapshot against the straight run");
        // Конфигурация мира портится ПЕРЕД возвратом намеренно: игра вправе крутить тяготение и
        // выключатель сна, и откат обязан вернуть их тоже. Без этой порчи оба поля снимка не
        // проверяются ничем — сцена их не трогает, и снимок без них проходил бы молча.
        w.set_sleep_enabled(!w.sleep_enabled());
        w.set_gravity({w.gravity().x + fix32::from_int(3), w.gravity().y});
        snap.apply(w);
        // Сверка идёт ДО следующего шага: ровно здесь видно поле, которое шаг перезаписал бы сам и
        // тем скрыл бы его пропажу из снимка.
        check_same(observe(w), at_take, "apply against the take point");
    }
}

// СЛОМАННАЯ РЕАЛИЗАЦИЯ возврата, написанная на публичном API: вернуть одни тела. Ровно та, которую
// пишут первой, и она обязана быть отбита. Без неё утверждения выше зелены вакуумно — сцена, чей
// исход не зависит от покоя, связности и кеша, пропустила бы и снимок из одних позиций.
void restore_bodies_only(World& w, const std::vector<Body>& saved) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(saved.size()); ++i) {
        Body& b = w.mutate(BodyId{i});
        b.position = saved[i].position;
        b.velocity = saved[i].velocity;
        b.angle = saved[i].angle;
        b.angular_velocity = saved[i].angular_velocity;
    }
}

void control_bodies_only(uint32_t at, const Observed& at_take, const Observed& at_end) {
    World w{32};
    scene::build(w);
    run(w, at);
    const std::vector<Body> saved = w.bodies();
    run(w, scene::RUN_TO - at);

    restore_bodies_only(w, saved);
    check(framework::physics::observed::first_difference(observe(w), at_take) != nullptr,
          "control: restoring only the bodies must NOT reproduce the take point");
    run(w, scene::RUN_TO - at);
    check(framework::physics::observed::first_difference(observe(w), at_end) != nullptr,
          "control: a run resumed from a bodies-only restore must diverge");
}

void gate_allocations() {
    World w{32};
    scene::build(w);
    run(w, scene::TAKE_AFTER_HIT);

    WorldSnapshot snap;

    // Холодное снятие ОБЯЗАНО тронуть кучу: ёмкости у снимка ещё нет. Утверждение стоит здесь
    // позитивным контролем самого измерения — ноль на прогретом снимке иначе неотличим от нуля на
    // снимке, который не копирует ничего.
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    snap.capture(w);
    const long cold = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(cold > 0, "the first capture of a scene pays for capacity");

    snap.apply(w);
    snap.capture(w);

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (int lap = 0; lap < 16; ++lap) {
        snap.apply(w);
        run(w, 1);
        snap.capture(w);
    }
    const long warm = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(warm == 0, "a warmed-up rollback tick does not touch the heap");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool counter_ok = framework::probe::control::plain_allocation();
    const long counter_seen = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(counter_ok && counter_seen > 0, "control: the counter sees a real allocation");

    std::printf("  allocs: cold=%ld warm=%ld (16 rollback ticks)\n", cold, warm);
}

} // namespace

int main() {
    std::printf("physics world snapshot: capture, apply, replay\n");
    const Reference ref = reference();
    std::printf("  reference: before=0x%016llx after=0x%016llx end=0x%016llx\n",
                static_cast<unsigned long long>(ref.before_hit.state),
                static_cast<unsigned long long>(ref.after_hit.state),
                static_cast<unsigned long long>(ref.end.state));
    gate_take(scene::TAKE_BEFORE_HIT, ref.before_hit, ref.end);
    gate_take(scene::TAKE_AT_HIT, ref.at_hit, ref.end);
    gate_take(scene::TAKE_AFTER_HIT, ref.after_hit, ref.end);
    control_bodies_only(scene::TAKE_AT_HIT, ref.at_hit, ref.end);
    control_bodies_only(scene::TAKE_AFTER_HIT, ref.after_hit, ref.end);
    gate_allocations();
    std::printf("framework-physics-snapshot: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
