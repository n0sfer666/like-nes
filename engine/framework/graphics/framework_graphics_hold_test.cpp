#include "framework_graphics_machine_scene.hpp"
#include "platform_args.hpp"

// Гейт ВРЕМЕНИ перехода (шаг B вертикали 1): прерываемость, «докрутить до конца кадра» и метки на
// тике переключения. Вопрос здесь мерится НОМЕРОМ ТИКА, а не номером состояния: машина, которая
// уходит в правильное состояние на тик раньше или позже, показывает лишний кадр — и это ровно тот
// дефект, который на глаз читается как «анимация дёргается», а по хешу не виден вовсе.
namespace {

using namespace scene;

const AnimTransition ALWAYS[1] = {{WALK, {}, 1}};
const AnimTransition ON_MOVE[1] = {{HIT, {MOVING, 0}, 1}};

// 1. Прерываемое состояние уходит первым же тиком — это пара ко всем задержкам ниже.
void test_interruptible() {
    Rig r(ALWAYS, 1, ANIM_STATE_INTERRUPTIBLE);
    same(r.leaves_at(IDLE, 0, 20), 1, "an interruptible state leaves on the very first tick");
}

// 2. «Докрутить кадр»: уход ждёт тика, на котором кадр и так сменился бы. Пара — тот же флаг на
// клипе из односчётных кадров, где ждать нечего.
void test_frame_end() {
    Rig r(ALWAYS, 1, ANIM_STATE_HOLD_UNTIL_FRAME_END);
    same(r.leaves_at(IDLE, 0, 20), 4, "the four-tick frame is played out, then the state leaves");

    Rig unit(ALWAYS, 1, ANIM_STATE_HOLD_UNTIL_FRAME_END);
    unit.states[IDLE].clip = WALK_CLIP;
    same(unit.leaves_at(IDLE, 0, 20), 1, "on unit-length frames there is nothing to play out");
}

// 3. «Докрутить клип»: непрерываемое состояние уходит по концу ПЕРИОДА, и каждый кадр показан ровно
// столько тиков, сколько записано в клипе. Второе утверждение отдельное: уход на тик позже даёт
// правильное состояние с лишним показом последнего кадра, и по номеру состояния он неразличим.
void test_hold_until_done() {
    Rig r(nullptr, 0, ANIM_STATE_INTERRUPTIBLE);
    r.states[HIT] = AnimStateDef{HIT_CLIP, ALWAYS, 1, ANIM_STATE_HOLD_UNTIL_DONE};
    r.start(HIT);

    uint32_t shown[8] = {0};
    uint32_t ticks = 0;
    for (uint32_t i = 0; i < 8 && r.m.current == HIT; ++i) {
        shown[ticks++] = machine_frame(r.m);
        r.step(0);
    }
    same(ticks, 3, "a three-frame once clip is held for exactly three ticks");
    same(shown[0], 0, "first tick shows the first frame");
    same(shown[1], 1, "second tick shows the second");
    same(shown[2], 2, "third tick shows the third, and it is not shown twice");
    same(r.m.current, WALK, "and then the state leaves");
}

// 4. Зацикленный клип под тем же флагом отпускает по концу первого круга, а не «никогда». Пара —
// он же прерываемым, который уходит сразу: без неё «ушёл на восьмом» было бы правдой и про машину,
// которая просто не умеет держать.
void test_loop_is_not_a_trap() {
    Rig held(ALWAYS, 1, ANIM_STATE_HOLD_UNTIL_DONE);
    same(held.leaves_at(IDLE, 0, 40), 8, "a looping clip releases after exactly one period");

    Rig free_(ALWAYS, 1, ANIM_STATE_INTERRUPTIBLE);
    same(free_.leaves_at(IDLE, 0, 40), 1, "the same data without the flag leaves at once");

    // Пустая шкала — ловушка того же рода: держать «до конца» в ней нечего, и состояние обязано
    // отпустить, а не ждать конца, которого нет.
    Rig empty(ALWAYS, 1, ANIM_STATE_HOLD_UNTIL_DONE);
    empty.states[IDLE].clip = Clip{};
    same(empty.leaves_at(IDLE, 0, 40), 1, "a state whose clip has no frames releases at once");
}

// 5. На тике переключения звучат метки НОВОГО клипа, а не старого. Пара — тот же тик того же клипа
// с несработавшим условием, где обязана прозвучать метка старого.
void test_events_on_switch() {
    AnimEvent buf[4] = {0, 0, 0, 0};

    Rig sw(nullptr, 0, ANIM_STATE_INTERRUPTIBLE);
    sw.states[WALK] = AnimStateDef{WALK_CLIP, ON_MOVE, 1, ANIM_STATE_HOLD_UNTIL_DONE};
    sw.start(WALK);
    uint32_t n = 0;
    for (uint32_t i = 0; i < 3; ++i) n = machine_step(sw.m, MOVING, buf, 4);
    same(sw.m.current, HIT, "the switch happens on the tick the walk cycle closes");
    same(n, 1, "exactly one mark is delivered on that tick");
    same(buf[0], 61, "and it is the first mark of the clip being entered");

    Rig stay(nullptr, 0, ANIM_STATE_INTERRUPTIBLE);
    stay.states[WALK] = AnimStateDef{WALK_CLIP, ON_MOVE, 1, ANIM_STATE_HOLD_UNTIL_DONE};
    stay.start(WALK);
    for (uint32_t i = 0; i < 3; ++i) n = machine_step(stay.m, 0, buf, 4);
    same(stay.m.current, WALK, "with the condition false the state stays");
    same(n, 1, "and the very same tick still delivers a mark");
    same(buf[0], 51, "but it is the mark of the clip that keeps playing");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics animation state machine: timing\n");
    test_interruptible();
    test_frame_end();
    test_hold_until_done();
    test_loop_is_not_a_trap();
    test_events_on_switch();
    std::printf("framework-graphics-hold: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
