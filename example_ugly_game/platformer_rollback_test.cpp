#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "platform_args.hpp"
#include "platformer_observed.hpp"
#include "platformer_rollback.hpp"
#include "platformer_sim.hpp"
#include "trajectory.hpp"

// Гейты 2 и 4 спеки #22 на НАСТОЯЩЕЙ сцене: тот же уровень из бандла, тот же контроллер и тот же
// маршрут, чей хеш прибит голденом, — но ввод приезжает с задержкой в шесть тиков, то есть каждый
// тик сначала играется по предсказанию и только потом подтверждается.
//
// Игрушечная симуляция (`framework_rollback_test`) этого не заменяет и не пытается: там состояние —
// две координаты и хеш, здесь — мир физики с контактами, раскладкой покоя, буфером событий и
// счётчиками работы плюс персонаж с окнами прощения и запомненной опорой. Снимок, потерявший
// что-нибудь из этого, на игрушке молчит по построению.
//
// Сети здесь по-прежнему нет ни строкой: задержка ввода есть то, ради чего откат существует, и
// транспорт добавит к ней потери и перестановку, не тронув ни одного здешнего утверждения.
namespace {

namespace ch = platformer::ch;

using platformer::Mark;
using platformer::difference;
using platformer::observe;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

constexpr uint32_t DELAY = 6;
constexpr uint32_t DEPTH = 8;
constexpr uint32_t CHECK_EVERY = 50;

// Первое расхождение НОМЕРОМ ТИКА и именем поля: «хеш не сошёлся» на четырёхстах тиках сцены не
// говорит, где смотреть.
struct Diff {
    int64_t tick = -1;
    const char* what = nullptr;
};

void reference(platformer::Stage& st, std::vector<Mark>& per_tick) {
    const uint32_t n = platformer::script_ticks();
    per_tick.reserve(n);
    for (uint32_t t = 0; t < n; ++t) {
        platformer::step_stage(st, platformer::script_input(t));
        per_tick.push_back(observe(st));
    }
}

// Прогон с опозданием: на тике `t` подтверждён ввод тика `t - DELAY`, а сам `t` играется
// предсказанием. `per_tick` не nullptr — сверять на контрольных точках, догнав долги.
Mark late(platformer::StageSim& sim, platformer::StageSession& ses,
          const std::vector<Mark>* per_tick, Diff* first_bad) {
    const uint32_t n = platformer::script_ticks();
    for (uint32_t t = 0; t < n; ++t) {
        if (t >= DELAY) ses.deliver(t - DELAY, 0, platformer::script_input(t - DELAY));
        ses.advance(sim);
        if (per_tick == nullptr || (t + 1) % CHECK_EVERY != 0) continue;
        for (uint32_t k = (t >= DELAY ? t - DELAY + 1 : 0); k <= t; ++k)
            ses.deliver(k, 0, platformer::script_input(k));
        ses.settle(sim);
        const char* d = difference(observe(*sim.stage), (*per_tick)[t]);
        if (d != nullptr && first_bad->tick < 0) {
            first_bad->tick = static_cast<int64_t>(t);
            first_bad->what = d;
        }
    }
    for (uint32_t k = n - DELAY; k < n; ++k) ses.deliver(k, 0, platformer::script_input(k));
    ses.settle(sim);
    return observe(*sim.stage);
}

bool load(const std::string& path, platformer::Stage& st, const char* what) {
    if (platformer::load_stage(path, st)) return true;
    check(false, what);
    return false;
}

// Контроль источника ввода, а не отката: потиковая ручка обязана давать ТОТ ЖЕ маршрут, что
// полосатый прогон, чей хеш прибит голденом. Разъедься они — все утверждения ниже говорили бы про
// маршрут, которого не проверяет никто.
void test_the_tick_input_walks_the_scripted_route(const std::string& path) {
    platformer::Stage scripted;
    platformer::Stage by_tick;
    if (!load(path, scripted, "the level loads for the scripted run")) return;
    if (!load(path, by_tick, "the level loads for the per-tick run")) return;

    const platformer::RunResult r = platformer::run_script(scripted, /*trace=*/false);
    ch::TrajectoryHash h;
    const uint32_t n = platformer::script_ticks();
    for (uint32_t t = 0; t < n; ++t) {
        platformer::step_stage(by_tick, platformer::script_input(t));
        h.feed(by_tick.hero);
    }
    check(r.ticks == n, "the per-tick source is as long as the scripted run");
    check(h.value == r.hash, "the per-tick source walks the very same route");
}

void test_a_late_run_matches_the_straight_one(const std::string& path,
                                             const std::vector<Mark>& per_tick) {
    platformer::Stage rolled;
    if (!load(path, rolled, "the level loads for the late run")) return;

    platformer::StageSim sim{&rolled};
    platformer::StageSession ses;
    ses.reset(1, DEPTH);
    Diff bad;
    const Mark got = late(sim, ses, &per_tick, &bad);

    if (bad.tick >= 0) {
        std::printf("  FAIL: the late run diverged at tick %lld on %s\n",
                    static_cast<long long>(bad.tick), bad.what);
        ++fails;
    }
    const char* end = difference(got, per_tick.back());
    if (end != nullptr) {
        std::printf("  FAIL: the late run ended on a different %s\n", end);
        ++fails;
    }
    // Без этих трёх утверждение выше проходит и прогон, в котором откатывать было нечего.
    check(ses.rollbacks() > 10, "control: that run really did roll back");
    check(ses.too_deep() == 0, "no confirmation arrived past the rollback depth");
    check(ses.conflicts() == 0, "no tick got two different confirmed inputs");
    std::printf("  late run: rollbacks=%u replayed=%u\n", ses.rollbacks(), ses.replayed());
}

// Негативный контроль: та же задержка при глубине ноль. Откатывать нечем, предсказание остаётся
// ошибкой навсегда — и прогон обязан РАЗОЙТИСЬ. Без него гейт выше зелен на сцене, которая сходится
// сама по себе.
void test_without_rollback_the_late_run_diverges(const std::string& path,
                                                const std::vector<Mark>& per_tick) {
    platformer::Stage stuck;
    if (!load(path, stuck, "the level loads for the depth-zero run")) return;

    platformer::StageSim sim{&stuck};
    platformer::StageSession ses;
    ses.reset(1, 0);
    Diff bad;
    const Mark got = late(sim, ses, nullptr, &bad);

    check(difference(got, per_tick.back()) != nullptr, "depth zero ends somewhere else entirely");
    check(ses.rollbacks() == 0, "depth zero rolls back nothing");
    check(ses.too_deep() > 0, "and says so: confirmations landed past the depth");
}

// Гейт 4 на ТЯЖЁЛОМ снимке: у мира физики внутри векторы, и откат, перевыделяющий их, стоил бы
// кадра. Ноль здесь означает, что `apply` пишет в уже выписанную ёмкость.
void test_a_rollback_tick_touches_no_heap(const std::string& path) {
    platformer::Stage st;
    if (!load(path, st, "the level loads for the allocation run")) return;

    platformer::StageSim sim{&st};
    platformer::StageSession ses;
    Diff bad;
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    ses.reset(1, DEPTH);
    late(sim, ses, nullptr, &bad);
    const long cold = framework::probe::allocs;
    framework::probe::in_hot = false;

    platformer::Stage again;
    if (!load(path, again, "the level loads for the warm run")) return;
    platformer::StageSim warm_sim{&again};
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    late(warm_sim, ses, nullptr, &bad);
    const long warm = framework::probe::allocs;
    framework::probe::in_hot = false;

    check(cold > 0, "control: the counter sees the snapshots being sized");
    check(warm == 0, "a warm run with rollbacks touches the heap zero times");
    check(ses.rollbacks() > 10, "control: that run really did roll back");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool got = framework::probe::control::plain_allocation();
    const long seen = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(got && seen > 0, "control: the counter sees a real allocation");
    std::printf("  allocs: cold=%ld warm=%ld (%u rollbacks)\n", cold, warm, ses.rollbacks());
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string path = DEFAULT_BUNDLE;
    for (int i = 1; i < argc; ++i) path = argv[i];

    std::printf("platformer sample: the scripted route under rollback\n");
    test_the_tick_input_walks_the_scripted_route(path);

    // Эталон считается ОДИН раз и на всех: два прогона по одному уровню обязаны совпадать по
    // построению (`platformer_sim_test` это и утверждает), и вторая его копия проверяла бы не
    // откат, а повторяемость загрузки.
    platformer::Stage straight;
    std::vector<Mark> per_tick;
    if (load(path, straight, "the level loads for the straight run")) {
        reference(straight, per_tick);
        test_a_late_run_matches_the_straight_one(path, per_tick);
        test_without_rollback_the_late_run_diverges(path, per_tick);
    }
    test_a_rollback_tick_touches_no_heap(path);
    std::printf("game-platformer-rollback: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
