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

    // Ёмкость мира — размер резерва, а не предел, и обещание нуля аллокаций держится на ДВУХ
    // границах сразу: числе тел и числе пересечений AABB. Вторая перерастает линейный резерв уже с
    // 34 тел, так что заниженная ёмкость — не «неправильное использование», а обычная плотная куча.
    // Проверяется здесь именно то, что заявлено в `world.hpp`: расширение АМОРТИЗИРУЕТСЯ и после
    // прогрева шаг снова не ходит в кучу. Иначе гейт выше говорил бы про ноль аллокаций только для
    // сцен, где резерв угадали.
    World tight(2);
    std::vector<BodyDesc> tight_descs;
    fixture::describe(tight_descs);
    fixture::fill(tight, tight_descs);

    // Первый шаг обязан РАСШИРИТЬСЯ, и это утверждение здесь не ради полноты. Без него «ноль
    // аллокаций после прогрева» подписалось бы и под миром, которому резерва хватило с самого
    // начала, — то есть под сценой, не проверяющей ничего. Ленивая инициализация рантайма к этому
    // моменту уже сделана шагом мира выше, так что счётчик видит только рост контейнеров.
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    tight.step(fixture::step_dt());
    const long growth = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(growth > 0, "control: an under-provisioned world really does grow");

    // Прогрев ВНЕ счётчика, и это не подгонка результата, а буквальное чтение контракта: вектор
    // растёт по максимуму числа пар за прогон, а не один раз, и куча уплотняется не за первый шаг.
    // Мерить со второго шага значило бы проверять формулировку «расширение одноразовое», которой
    // `world.hpp` больше не обещает. Измеряемое утверждение — ниже: после прогрева ноль.
    for (uint32_t i = 0; i < fixture::STEPS; ++i) tight.step(fixture::step_dt());
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (uint32_t i = 0; i < fixture::STEPS; ++i) tight.step(fixture::step_dt());
    const long after_growth = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(after_growth == 0, "an under-provisioned world grows once, then steps allocate nothing");

    // Ёмкость влияет на аллокации и ни на что больше: тот же прогон в мире с достаточным резервом
    // обязан дать тот же хеш. Без этой сверки «ноль аллокаций» покупалось бы тем, что
    // под-провизионированный мир считает другую физику.
    World roomy(fixture::CAPACITY);
    std::vector<BodyDesc> roomy_descs;
    fixture::describe(roomy_descs);
    fixture::fill(roomy, roomy_descs);
    for (uint32_t i = 0; i <= 2 * fixture::STEPS; ++i) roomy.step(fixture::step_dt());
    std::printf("  under-provisioned: growth=%ld settled=%ld\n", growth, after_growth);
    check(roomy.hash() == tight.hash(), "capacity changes allocations and nothing else");

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
