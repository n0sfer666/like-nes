#include <cstdint>
#include <cstdio>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "framework_rollback_toy.hpp"
#include "session.hpp"

// Гейт 2 спеки #22 на игрушечной симуляции: прогон, где чужой ввод приезжает с задержкой, обязан
// дать ТО ЖЕ состояние, что прогон без задержки. Плюс гейт 4 — ноль обращений к куче в тике отката.
//
// Сети здесь нет ни строкой, и это не упрощение: `deliver` означает «ввод стал известен», а
// задержка — то, ради чего откат существует. Транспорт добавит к этому потери и перестановку, но
// не изменит ни одного из здешних утверждений.
namespace {

using framework::rollback::Session;
using framework::rollback::Tick;
using namespace framework::rollback::toy;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

constexpr uint32_t TICKS = 200;
constexpr uint32_t DELAY = 6;
constexpr uint32_t DEPTH = 8;
constexpr uint32_t CHECK_EVERY = 25;

using InputFn = ToyInput (*)(uint32_t, uint32_t);

// Эталон: оба ввода известны сразу, откату работы нет. Он же печатает состояние ПОТИКОВО — иначе
// расхождение отката называется одним словом «хеш не сошёлся», а нужен номер первого разошедшегося
// тика (то же требование, что гейт 6 спеки предъявляет верификации реплея).
uint64_t reference(InputFn gen, uint64_t* per_tick) {
    ToySim sim;
    ToyInput in[ToySim::PLAYERS];
    for (Tick t = 0; t < TICKS; ++t) {
        for (uint32_t p = 0; p < ToySim::PLAYERS; ++p) in[p] = gen(t, p);
        sim.step(in);
        per_tick[t] = sim.state.hash;
    }
    return sim.state.hash;
}

// Прогон с задержкой: игрок 0 нажимает сейчас, ввод игрока 1 приезжает на `DELAY` тиков позже.
// `per_tick` не nullptr — сверять на каждой контрольной точке, догнав долги; `first_bad` получает
// номер первого разошедшегося тика.
template <class Sim>
uint64_t delayed(Sim& sim, Session<Sim>& ses, InputFn gen,
                 const uint64_t* per_tick, int64_t* first_bad) {
    for (Tick t = 0; t < TICKS; ++t) {
        ses.deliver(t, 0, gen(t, 0));
        if (t >= DELAY) ses.deliver(t - DELAY, 1, gen(t - DELAY, 1));
        ses.advance(sim);
        if (per_tick == nullptr || (t + 1) % CHECK_EVERY != 0) continue;
        for (Tick k = (t >= DELAY ? t - DELAY + 1 : 0); k <= t; ++k) ses.deliver(k, 1, gen(k, 1));
        ses.settle(sim);
        if (sim.state.hash != per_tick[t] && *first_bad < 0) *first_bad = static_cast<int64_t>(t);
    }
    for (Tick k = TICKS - DELAY; k < TICKS; ++k) ses.deliver(k, 1, gen(k, 1));
    ses.settle(sim);
    return sim.state.hash;
}

void test_a_delayed_run_matches_the_straight_one() {
    uint64_t per_tick[TICKS];
    const uint64_t want = reference(scripted, per_tick);

    ToySim sim;
    Session<ToySim> ses;
    ses.reset(ToySim::PLAYERS, DEPTH);
    int64_t first_bad = -1;
    const uint64_t got = delayed(sim, ses, scripted, per_tick, &first_bad);

    if (first_bad >= 0) std::printf("  first divergent tick: %lld\n", static_cast<long long>(first_bad));
    check(first_bad < 0, "every checkpoint of the delayed run matches the straight one");
    check(got == want, "and so does the final state");
    // Предпосылка гейта, без которой он зелен вакуумно: откаты ДЕЙСТВИТЕЛЬНО случались. Прогон, в
    // котором предсказание угадало всё, проходит сверку хешей, ни разу не тронув переигрывание.
    check(ses.rollbacks() >= 20, "control: the delay really forced rollbacks");
    check(ses.replayed() >= ses.rollbacks(), "and each rollback replayed at least one tick");
    check(ses.too_deep() == 0, "no input arrived deeper than the ring");
    check(ses.conflicts() == 0, "and no tick got two different confirmed inputs");
    std::printf("  scripted: rollbacks=%u replayed=%u\n", ses.rollbacks(), ses.replayed());
}

// Позитивный контроль первого гейта: без отката тот же прогон обязан РАЗОЙТИСЬ. Ломается
// конфигурацией, а не подменённым кодом, — глубина ноль означает «вернуться некуда», и это тот же
// путь исполнения, что в бою.
void test_without_rollback_the_delayed_run_diverges() {
    uint64_t per_tick[TICKS];
    const uint64_t want = reference(scripted, per_tick);

    ToySim sim;
    Session<ToySim> ses;
    ses.reset(ToySim::PLAYERS, 0);
    int64_t first_bad = -1;
    const uint64_t got = delayed(sim, ses, scripted, nullptr, &first_bad);

    check(got != want, "control: with depth 0 the delayed run does not match");
    check(ses.rollbacks() == 0, "and no rollback happened at all");
    check(ses.too_deep() > 0, "while the refusals are counted, not swallowed");
}

// Второй контроль, теперь по состоянию: снимок, забывший накопленное поле, обязан быть отбит.
// Первый контроль ломает решение об откате, этот — его материал; сломанным может оказаться любой
// из двух, а расхождение хеша у них одно на двоих.
void test_a_lossy_restore_is_rejected() {
    uint64_t per_tick[TICKS];
    const uint64_t want = reference(scripted, per_tick);

    LossySim sim;
    Session<LossySim> ses;
    ses.reset(ToySim::PLAYERS, DEPTH);
    int64_t first_bad = -1;
    const uint64_t got = delayed(sim, ses, scripted, nullptr, &first_bad);

    check(got != want, "control: a restore that drops a field does not match");
    check(ses.rollbacks() > 0, "and it is the rollback path that was taken");
}

// Верная догадка стоит ноль переигранных тиков. Реализация, откатывающаяся на каждый пришедший
// ввод, проходит все три гейта выше и валит бюджет кадра — то есть краснеет только на машине
// владельца, где чинить дороже всего.
void test_a_correct_prediction_costs_nothing() {
    uint64_t per_tick[TICKS];
    const uint64_t want = reference(steady, per_tick);

    ToySim sim;
    Session<ToySim> ses;
    ses.reset(ToySim::PLAYERS, DEPTH);
    int64_t first_bad = -1;
    const uint64_t got = delayed(sim, ses, steady, per_tick, &first_bad);

    check(got == want, "a steady stream still matches the straight run");
    check(first_bad < 0, "at every checkpoint too");
    // Ровно один: первый тик предсказан умолчанием (истории ещё нет), и приехавшая правда его
    // опровергает. Дальше предсказание — повтор подтверждённого, и оно верно всегда.
    check(ses.rollbacks() == 1, "and only the very first tick is ever replayed");
    check(ses.replayed() == DELAY, "which costs exactly the delay in replayed ticks");
}

// Гейт 4: тик отката не трогает кучу. Кольца выписаны один раз, снимок — присваивание в готовый
// слот, предсказание — обход по кольцу.
void test_a_rollback_tick_touches_no_heap() {
    ToySim warm_sim;
    Session<ToySim> ses;
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    ses.reset(ToySim::PLAYERS, DEPTH);
    const long cold = framework::probe::allocs;
    framework::probe::in_hot = false;
    int64_t first_bad = -1;
    delayed(warm_sim, ses, scripted, nullptr, &first_bad);

    ToySim sim;
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    ses.reset(ToySim::PLAYERS, DEPTH);
    delayed(sim, ses, scripted, nullptr, &first_bad);
    const long warm = framework::probe::allocs;
    framework::probe::in_hot = false;

    // Позитивный контроль измерения, а не сессии: счётчик, который не считает, показывает ноль на
    // любом коде. Холодная выписка колец обязана быть им ВИДНА.
    check(cold > 0, "control: the counter sees the rings being allocated");
    check(warm == 0, "a warm run with rollbacks touches the heap zero times");
    check(ses.rollbacks() > 0, "control: that run really did roll back");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool got = framework::probe::control::plain_allocation();
    const long seen = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(got && seen > 0, "control: the counter sees a real allocation");
    std::printf("  allocs: cold=%ld warm=%ld (%u rollbacks)\n", cold, warm, ses.rollbacks());
}

} // namespace

int main() {
    test_a_delayed_run_matches_the_straight_one();
    test_without_rollback_the_delayed_run_diverges();
    test_a_lossy_restore_is_rejected();
    test_a_correct_prediction_costs_nothing();
    test_a_rollback_tick_touches_no_heap();
    std::printf("framework-rollback: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
