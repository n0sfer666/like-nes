#include <cstdio>

#include "plan.hpp"

// Гейт плана переигрывания: арифметика отката без единого шага симуляции.
//
// Отдельной целью от гейта сессии, потому что ломается она иначе. Сессия отвечает «прогон с
// задержкой сошёлся с прогоном без неё», и на этот вопрос откат, промахнувшийся на один тик, чаще
// всего отвечает РАСХОЖДЕНИЕМ ХЕША — то есть тем же словом, что и сломанный снимок, сломанное
// предсказание и сломанная симуляция. Здесь же каждое правило названо по имени.
namespace {

using framework::rollback::ReplayPlan;
using framework::rollback::Tick;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

ReplayPlan at(Tick head, uint32_t depth) {
    ReplayPlan p;
    p.reset(depth);
    for (Tick t = 0; t < head; ++t) p.advanced();
    return p;
}

// Ввод, приехавший на ещё не сыгранный тик, откатом не является. Правило выглядит очевидным ровно
// до реализации, которая считает «испорченным» любой пришедший ввод: она откатывается на каждом
// кадре и остаётся ЗЕЛЁНОЙ по хешу, потому что переигрывает то же самое. Стоимость такого отката
// видна только гейтом 8 (бюджет кадра), то есть на машине владельца.
void test_future_input_is_not_a_rollback() {
    ReplayPlan p = at(10, 8);
    check(p.note_dirty(10), "input for the current tick is accepted");
    check(p.note_dirty(12), "input for a future tick is accepted");
    check(!p.pending(), "neither of them plans a replay");
    check(p.replay_count() == 0, "and neither of them replays a tick");
}

// Побеждает самый ранний. Реализация, оставляющая последний, переигрывает с тика, который уже
// посчитан испорченным вводом, — и расходится ровно на ту часть прогона, что между ними.
void test_the_earliest_dirty_tick_wins() {
    ReplayPlan p = at(20, 8);
    check(p.note_dirty(17), "a late input at 17");
    check(p.note_dirty(19), "a later one at 19 arrives in the same frame");
    check(p.note_dirty(15), "and an earlier one at 15 after them");
    check(p.from() == 15, "the plan returns to the earliest of the three");
    check(p.replay_count() == 5, "and replays 15..19, five ticks");

    p.rewind();
    check(p.head() == 15, "rewind puts the clock on the tick to be replayed");
    check(!p.pending(), "and clears the plan");
    check(p.replay_count() == 0, "a cleared plan replays nothing");
}

// Граница глубины — на самой границе, а не в середине диапазона. Ошибка на единицу здесь означает
// возврат в снимок, уже вытесненный из кольца: восстановится ЧУЖОЕ состояние, и прогон продолжится
// молча, потому что состояние по форме верное.
void test_the_depth_boundary_holds_on_both_sides() {
    ReplayPlan p = at(100, 8);
    check(p.note_dirty(92), "a tick exactly `depth` back is still reachable");
    check(p.too_deep() == 0, "and is not counted as too deep");
    check(!p.note_dirty(91), "one tick further back is not");
    check(p.too_deep() == 1, "and it is counted, not swallowed");
    check(p.from() == 92, "the rejected tick does not widen the plan");
    check(p.replay_count() == 8, "which still replays 92..99");
}

// Нулевая глубина — рабочая конфигурация позитивного контроля гейта сессии: на ней откатов нет
// ВОВСЕ, и гейт совпадения хешей обязан на ней краснеть. Если она молча ведёт себя как глубина в
// один тик, контроль зелен вакуумно и ничего не доказывает.
void test_zero_depth_rolls_back_nothing() {
    ReplayPlan p = at(5, 0);
    check(!p.note_dirty(4), "with depth 0 even the previous tick is out of reach");
    check(p.too_deep() == 1, "and the refusal is counted");
    check(!p.pending(), "nothing is planned");
    check(p.note_dirty(5), "while the current tick is still accepted");
}

} // namespace

int main() {
    test_future_input_is_not_a_rollback();
    test_the_earliest_dirty_tick_wins();
    test_the_depth_boundary_holds_on_both_sides();
    test_zero_depth_rolls_back_nothing();
    std::printf("framework-rollback-plan: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
