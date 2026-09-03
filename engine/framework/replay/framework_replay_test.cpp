#include <cstdio>

#include "framework_rollback_toy.hpp"
#include "verify.hpp"

// Гейт 6 спеки #22 на игрушечной симуляции: честный поток принимается, подделанный отбивается С
// НОМЕРОМ ТИКА.
//
// Игрушка НАМЕРЕННО, по тому же основанию, что и у гейта сессии: вопрос здесь про верификатор — про
// то, на каком тике он замечает расхождение и умеет ли назвать его, — а сцена на fix32 отвечала бы
// расхождением хеша и на сломанном верификаторе, и на сломанном снимке, и на сломанном решателе.
// Настоящая сцена проверяется гейтом игры-образца, и утверждение там другое.
namespace {

namespace toy = framework::rollback::toy;
namespace replay = framework::replay;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

constexpr uint32_t TICKS = 200;
constexpr replay::Tick FORGED_AT = 137;

// Запись прогона: та же симуляция, тот же скрипт. Заявленный хеш берётся ПОСЛЕ шага — тот же
// момент, в который его сверяет верификатор.
void record(replay::Stream<toy::ToyInput>& out) {
    toy::ToySim sim;
    out.reset(toy::ToySim::PLAYERS);
    for (uint32_t t = 0; t < TICKS; ++t) {
        toy::ToyInput row[toy::ToySim::PLAYERS];
        for (uint32_t p = 0; p < toy::ToySim::PLAYERS; ++p) row[p] = toy::scripted(t, p);
        sim.step(row);
        out.record(row, sim.hash());
    }
}

// Сломанный верификатор №1: сверяет только ПОСЛЕДНИЙ тик. Подделку он находит — накопленный хеш
// разошёлся, — но назвать её номер не может по построению. Без этого контроля утверждение «отбит
// на тике K» неотличимо от «отбит».
template <class Sim>
replay::Verdict verify_final_only(Sim& sim, const replay::Stream<typename Sim::Input>& s) {
    for (replay::Tick t = 0; t < s.ticks(); ++t) sim.step(s.row(t));
    if (s.ticks() == 0 || sim.hash() != s.claim(s.ticks() - 1))
        return replay::Verdict{replay::Reason::Diverged, s.ticks() - 1};
    return replay::Verdict{replay::Reason::Match, s.ticks()};
}

// Сломанный верификатор №2: переигрывает и не сверяет вовсе. Ровно та реализация, при которой
// «поток принят» печатается всегда, — и она обязана быть отбита утверждением о подделке.
template <class Sim>
replay::Verdict verify_blind(Sim& sim, const replay::Stream<typename Sim::Input>& s) {
    for (replay::Tick t = 0; t < s.ticks(); ++t) sim.step(s.row(t));
    return replay::Verdict{replay::Reason::Match, s.ticks()};
}

void test_an_honest_stream_is_accepted(const replay::Stream<toy::ToyInput>& s) {
    toy::ToySim sim;
    const replay::Verdict v = replay::verify(sim, s, toy::ToySim::PLAYERS);
    check(v.ok(), "an honest stream is accepted");
    // Число проверенных тиков — не отладка: «принят» без него верен и для потока, в котором
    // проверять было нечего.
    check(v.tick == TICKS, "and says how many ticks it checked");
}

void test_a_forged_input_is_named_by_tick(replay::Stream<toy::ToyInput> s) {
    toy::ToyInput other = toy::scripted(FORGED_AT, 0);
    other.dx += 1;
    check(s.forge_input(FORGED_AT, 0, other), "the forge touches a tick that exists");

    toy::ToySim sim;
    const replay::Verdict v = replay::verify(sim, s, toy::ToySim::PLAYERS);
    check(!v.ok() && v.reason == replay::Reason::Diverged, "a forged input is refused");
    check(v.tick == FORGED_AT, "and the refusal names the very tick that was forged");

    // Контроль читается наоборот, и это не описка: слепой верификатор обязан подделку ПРИНЯТЬ.
    // Отказ настоящего значит что-то ровно потому, что реализация без сверки на том же потоке
    // печатает «принят».
    toy::ToySim blind_sim;
    check(verify_blind(blind_sim, s).ok(), "control: a verifier that checks nothing accepts it");
    toy::ToySim final_sim;
    const replay::Verdict f = verify_final_only(final_sim, s);
    check(!f.ok(), "control: the final-hash verifier does notice the forgery");
    check(f.tick != FORGED_AT, "control: but it cannot name the tick, which is why claims are per tick");
}

void test_a_forged_claim_is_named_by_tick(replay::Stream<toy::ToyInput> s) {
    check(s.forge_claim(FORGED_AT, s.claim(FORGED_AT) ^ 1ull), "the forged claim exists");
    toy::ToySim sim;
    const replay::Verdict v = replay::verify(sim, s, toy::ToySim::PLAYERS);
    check(!v.ok() && v.tick == FORGED_AT, "a forged claim is refused at its own tick");
}

// Пустой поток — отказ, а не «совпало». Верификатор, отвечающий «принят» на потоке из нуля тиков,
// выдаёт зелёный вердикт о том, чего не было: тот же класс, что правило `vacuous-gate` в линтере.
void test_nothing_is_not_a_match() {
    replay::Stream<toy::ToyInput> empty;
    empty.reset(toy::ToySim::PLAYERS);
    toy::ToySim sim;
    const replay::Verdict v = replay::verify(sim, empty, toy::ToySim::PLAYERS);
    check(!v.ok() && v.reason == replay::Reason::Empty, "an empty stream is not a match");
}

// Ширина строки — часть контракта `step`: поток от другого числа игроков поехал бы в симуляцию
// сдвинутым, и расхождение читалось бы как подделка.
void test_a_stream_of_another_width_is_refused(const replay::Stream<toy::ToyInput>& s) {
    toy::ToySim sim;
    const replay::Verdict v = replay::verify(sim, s, toy::ToySim::PLAYERS + 1);
    check(!v.ok() && v.reason == replay::Reason::Players, "a stream of another width is refused");
}

// Тот же поток, но переигранный симуляцией с ДРУГИМ поведением. Реплей проверяет не байты, а то,
// что прогон воспроизводим нашим движком: сборка, шагающая иначе, обязана быть отбита.
void test_another_simulation_is_refused(const replay::Stream<toy::ToyInput>& s) {
    struct DriftingSim : toy::ToySim {
        void step(const Input* in) {
            toy::ToySim::step(in);
            state.hash ^= 1ull;
        }
    };
    DriftingSim sim;
    const replay::Verdict v = replay::verify(sim, s, toy::ToySim::PLAYERS);
    check(!v.ok() && v.tick == 0, "a simulation that steps differently is refused at the first tick");
}

} // namespace

int main() {
    replay::Stream<toy::ToyInput> honest;
    record(honest);
    check(honest.ticks() == TICKS, "the recorder wrote a tick per step");

    test_an_honest_stream_is_accepted(honest);
    test_a_forged_input_is_named_by_tick(honest);
    test_a_forged_claim_is_named_by_tick(honest);
    test_nothing_is_not_a_match();
    test_a_stream_of_another_width_is_refused(honest);
    test_another_simulation_is_refused(honest);

    std::printf("framework-replay: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
