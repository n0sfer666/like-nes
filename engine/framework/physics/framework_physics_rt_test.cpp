#include <cstdio>
#include <vector>

#include "framework_alloc_probe.hpp"
#include "framework_physics_scene.hpp"
#include "platform_args.hpp"

// Гейт 6 спеки #15: шаг физики не ходит в кучу. Вся память выделяется конструктором мира, всё
// остальное живёт в уже выделенном. Счётчик — общий с гейтом 7 спеки #14, одной реализацией:
// две копии одного харнесса считали бы РАЗНОЕ под одним названием.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

} // namespace

int main(int argc, char** argv) {
    using namespace framework::physics;
    platform::Args args(argc, argv);
    std::printf("framework physics runtime gate\n");

    World w(fixture::CAPACITY);
    std::vector<BodyDesc> descs;
    fixture::describe(descs);
    fixture::fill(w, descs);

    // Первый шаг прогоняется ДО счётчика: он законно трогает ленивую инициализацию рантайма
    // (буферы stdio, локаль), и считать её аллокацией шага значило бы получить красный гейт,
    // ничего не говорящий про физику.
    w.step(fixture::step_dt());

    framework::probe::in_hot = true;
    for (uint32_t i = 0; i < fixture::STEPS; ++i) w.step(fixture::step_dt());
    const long during_step = framework::probe::allocs;

    // Хеш меряется отдельно и тоже обязан быть бесплатным: его зовут каждый кадр сетевой
    // прогноз и запись реплея, и аллокация там стоила бы ровно столько же, сколько в шаге.
    framework::probe::allocs = 0;
    const uint64_t h = w.hash();
    const long during_hash = framework::probe::allocs;
    framework::probe::in_hot = false;

    std::printf("  allocs: step=%ld hash=%ld, hash=0x%016llx\n", during_step, during_hash,
                static_cast<unsigned long long>(h));
    check(during_step == 0, "step() allocates nothing");
    check(during_hash == 0, "hash() allocates nothing");

    // Позитивный контроль счётчика: без него ноль аллокаций неотличим от неработающего
    // перехвата — а неработающий перехват выглядит как самый зелёный гейт на свете.
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    std::vector<int>* leak = new std::vector<int>();
    leak->resize(64);
    const long control = framework::probe::allocs;
    framework::probe::in_hot = false;
    delete leak;
    check(control > 0, "control: the counter sees a real allocation");

    std::printf("framework-physics-rt: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
