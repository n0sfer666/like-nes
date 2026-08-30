#include "framework_graphics_machine_scene.hpp"
#include "platform_args.hpp"

// Гейт ВЫБОРА перехода (шаг B вертикали 1): условия, приоритеты, переход в себя.
//
// Отделён от гейта времени, потому что «выбрала не тот переход» и «переключилась не в тот тик» —
// разные отказы с разными уликами: первый виден по номеру состояния, второй только по номеру тика.
//
// Каждое утверждение парное: «переход сработал» — правда и про машину, которая переключается на что
// угодно, поэтому рядом всегда стоит прогон, где тот же переход обязан НЕ сработать.
namespace {

using namespace scene;

// 1. Обе маски условия несут вес, и каждая — в обе стороны.
void test_condition() {
    const AnimCondition need{MOVING, 0};
    check(anim_condition_holds(need, MOVING), "all: the required bit passes");
    check(!anim_condition_holds(need, GROUND), "all: without the required bit it fails");

    const AnimCondition forbid{0, HURT};
    check(anim_condition_holds(forbid, MOVING), "none: without the forbidden bit it passes");
    check(!anim_condition_holds(forbid, HURT), "none: the forbidden bit fails it");

    const AnimCondition both{MOVING | GROUND, HURT};
    check(anim_condition_holds(both, MOVING | GROUND), "both masks: exactly the wanted bits pass");
    check(!anim_condition_holds(both, MOVING), "both masks: one required bit missing fails");
    check(!anim_condition_holds(both, MOVING | GROUND | HURT), "both masks: a forbidden bit fails");
    check(anim_condition_holds(AnimCondition{}, 0), "an empty condition holds on empty flags");
}

// 2. Приоритет решает, и пара к нему — тот же набор с обменянными приоритетами.
void test_priority() {
    const AnimTransition low_walk[2] = {{WALK, {MOVING, 0}, 1}, {HIT, {MOVING, 0}, 2}};
    Rig a(low_walk, 2, ANIM_STATE_INTERRUPTIBLE);
    a.start(IDLE);
    a.step(MOVING);
    same(a.m.current, HIT, "the higher priority transition wins");

    const AnimTransition low_hit[2] = {{WALK, {MOVING, 0}, 2}, {HIT, {MOVING, 0}, 1}};
    Rig b(low_hit, 2, ANIM_STATE_INTERRUPTIBLE);
    b.start(IDLE);
    b.step(MOVING);
    same(b.m.current, WALK, "swapping the priorities swaps the winner");

    // Условие важнее приоритета: самый приоритетный, но не подошедший, не участвует вовсе.
    Rig c(low_walk, 2, ANIM_STATE_INTERRUPTIBLE);
    c.start(IDLE);
    c.step(0);
    same(c.m.current, IDLE, "no condition holds, so nothing fires");
}

// 3. Равный приоритет — выигрывает ПЕРВЫЙ по списку. Пара: перевёрнутый список даёт другого.
void test_order() {
    const AnimTransition walk_first[2] = {{WALK, {MOVING, 0}, 1}, {HIT, {MOVING, 0}, 1}};
    Rig a(walk_first, 2, ANIM_STATE_INTERRUPTIBLE);
    a.start(IDLE);
    a.step(MOVING);
    same(a.m.current, WALK, "on equal priority the first listed transition wins");

    const AnimTransition hit_first[2] = {{HIT, {MOVING, 0}, 1}, {WALK, {MOVING, 0}, 1}};
    Rig b(hit_first, 2, ANIM_STATE_INTERRUPTIBLE);
    b.start(IDLE);
    b.step(MOVING);
    same(b.m.current, HIT, "reversing the list reverses the winner");
}

// 4. Переход в себя не перезапускает клип. Пара — явный `machine_start`, который перезапускает.
void test_self() {
    const AnimTransition to_self[1] = {{IDLE, {MOVING, 0}, 1}};
    Rig r(to_self, 1, ANIM_STATE_INTERRUPTIBLE);
    r.start(IDLE);
    for (uint32_t i = 0; i < 5; ++i) r.step(MOVING);
    same(r.m.current, IDLE, "a self-transition keeps the state");
    same(r.m.player.elapsed, 5, "a self-transition does not restart the clip");
    same(machine_frame(r.m), 1, "five ticks into a four-tick frame means the second frame");

    r.start(IDLE);
    same(r.m.player.elapsed, 0, "an explicit start does restart it");
    same(machine_frame(r.m), 0, "and puts it back on the first frame");
}

// 5. Испорченные данные не выводят машину за массив: номер состояния вне набора — не переход.
void test_bounds() {
    const AnimTransition nowhere[2] = {{9, {MOVING, 0}, 5}, {WALK, {MOVING, 0}, 1}};
    Rig r(nowhere, 2, ANIM_STATE_INTERRUPTIBLE);
    r.start(IDLE);
    r.step(MOVING);
    same(r.m.current, WALK, "a transition out of range is skipped, not taken");

    Rig empty(nullptr, 0, ANIM_STATE_INTERRUPTIBLE);
    empty.m.states = nullptr;
    same(machine_start(empty.m, IDLE, nullptr, 0), 0, "a machine without states starts nothing");
    same(empty.m.current, ANIM_STATE_NONE, "and lands in no state at all");
    same(machine_step(empty.m, MOVING, nullptr, 0), 0, "and stepping it does nothing");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics animation state machine: choice\n");
    test_condition();
    test_priority();
    test_order();
    test_self();
    test_bounds();
    std::printf("framework-graphics-pick: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
