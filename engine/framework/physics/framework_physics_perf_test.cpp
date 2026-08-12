#include <cstdint>
#include <cstdio>

// Подмена глобальных `operator new`/`delete` живёт здесь, а не в измерительном заголовке: её
// контракт — ровно один TU на программу, и владеть им обязан тот, кто программу и составляет.
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "framework_physics_load.hpp"
#include "framework_physics_perf_probe.hpp"
#include "platform_args.hpp"

// Гейт 8 спеки #15: целевое число активных тел укладывается в бюджет кадра.
//
// Утверждается здесь НЕ время, а работа: счётчики шага (`counters.hpp`) детерминированы и обязаны
// совпасть на трёх ОС до единицы, а стенные часы на раннере GitHub гуляют кратно — красный шаг
// «кадр занял 3.1 мс вместо 2» отключили бы как флейкующий, и вместе с ним ушёл бы весь гейт. Время
// печатается в лог и попадает в спеку с названием машины; сравнение с бюджетом делает владелец на
// живом железе (правило 18).
//
// Утверждений ТРИ рода, и смешивать их нельзя. **Эталон** — точное равенство шести счётчикам,
// голден: ловит расхождение между ОС, между Debug и Release и любой сдвиг поведения шага. Живёт он
// ЗДЕСЬ, а не грепом по выводу в шаге CI: греп по литералу в workflow не проверяется ничем локально,
// дублируется руками в доках и всплывает только после пуша и двадцати минут прогона на трёх ОС.
// **Структурные потолки** (`check_bounded`) выведены из алгоритма — каждая ступень шага ограничена
// предыдущей — и потому переживают перепин эталона: перепин операция механическая, и через несколько
// итераций эталон описывает уже не задуманное, а получившееся. **Доля перебора** (`check_share`)
// структурной НЕ является и здесь так и называется.
namespace {

using framework::physics::perf::Bounds;
using framework::physics::perf::full_sweep;
using framework::physics::perf::Peak;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void check_named(const char* scene, const char* claim, bool ok) {
    char what[160];
    std::snprintf(what, sizeof(what), "%s: %s", scene, claim);
    check(ok, what);
}

// Эталон — МАКСИМУМ счётчика по ФИКСИРОВАННОМУ окну [`WARMUP`, `WARMUP` + `MEASURED`), а не
// асимптотика сцены: куча продолжает микрооседать и на тысячном кадре, то есть никакого «наконец
// установившегося» значения у неё нет. Именно поэтому здесь точное равенство, а не потолок: потолок
// пришлось бы ставить выше недостижимой асимптотики и он не сторожил бы ничего, а равенство по
// фиксированному окну детерминировано и сравнимо между ОС.
struct Reference {
    uint64_t bodies = 0;
    uint64_t pairs = 0;
    uint64_t broad_candidates = 0;
    uint64_t narrow_checks = 0;
    uint64_t velocity_projections = 0;
    uint64_t position_projections = 0;
};

void expect(const char* scene, const char* field, uint64_t got, uint64_t want) {
    if (got == want) return;
    std::printf("  FAIL: %s: %s = %llu, reference says %llu\n", scene, field,
                static_cast<unsigned long long>(got), static_cast<unsigned long long>(want));
    ++fails;
}

void check_reference(const char* scene, const Peak& p, const Reference& r) {
    expect(scene, "bodies", p.active_bodies, r.bodies);
    expect(scene, "pairs", p.pairs, r.pairs);
    expect(scene, "broad", p.broad_candidates, r.broad_candidates);
    expect(scene, "narrow", p.narrow_checks, r.narrow_checks);
    expect(scene, "vel", p.velocity_projections, r.velocity_projections);
    expect(scene, "pos", p.position_projections, r.position_projections);
}

// Инвариант 6 спеки («объём работы на шаг ограничен сверху») в структурной форме. На колонне три
// последних утверждения проходят даром: контактов там нет, и ноль укладывается в любой потолок —
// сторожит её позитивный контроль ниже, а не они.
void check_bounded(const char* scene, const Peak& p) {
    check_named(scene, "broadphase stays within the full pairwise sweep", !p.breach.broad_sweep);
    check_named(scene, "emitted pairs stay within the candidates seen", !p.breach.pairs_vs_broad);
    check_named(scene, "narrowphase runs no more often than there are pairs",
                !p.breach.narrow_vs_pairs);
    check_named(scene, "velocity work stays within points times iterations", !p.breach.velocity);
    check_named(scene, "position work stays within points times iterations", !p.breach.position);
}

// Доля полного перебора — порог ИЗ ЗАМЕРА, поставленный примерно вдвое выше измеренного, а не
// следствие алгоритма. Названо прямо, потому что выдать подогнанное число за структурное — способ
// получить гейт, который сторожит собственную историю: он сработает на смене КЛАССА (широкая фаза
// выродилась в перебор каждого с каждым при живой сортировке), но не на двадцати процентах роста, и
// обещать он должен ровно это. Точную цифру держит эталон, он же и падает первым.
void check_share(const char* scene, const Peak& p) {
    check_named(scene, "broadphase stays inside the share the measurement showed",
                !p.breach.broad_share);
}

} // namespace

int main(int argc, char** argv) {
    using namespace framework::physics;
    platform::Args args(argc, argv);
    std::printf("framework physics perf gate (%u bodies)\n", load::BODIES);

    // Ёмкость мира — ровно по числу тел сцены: гейт про цену шага обязан мерить шаг, а не разовое
    // расширение векторов, а «с запасом» означало бы второй литерал для той же тройки статических
    // тел. Что расширение стоит аллокаций и что после прогрева их ноль, утверждает гейт 6
    // (`framework_physics_rt_test`); здесь — что на ЭТИХ сценах ноль тоже держится.
    World heap_world(load::BODIES + load::HEAP_STATICS);
    const uint32_t heap_statics = load::heap(heap_world, load::BODIES);
    World scatter_world(load::BODIES);
    load::scatter(scatter_world, load::BODIES);
    World column_world(load::BODIES);
    load::column(column_world, load::BODIES);

    // Ленивую инициализацию рантайма трогает первый же шаг, и считать её аллокацией шага значило бы
    // получить красный гейт, ничего не говорящий про физику. Отдельного шага для этого не нужно:
    // счётчик включается ПОСЛЕ прогрева, а прогрев в триста кадров идёт до него.
    const Peak heap = perf::measure(heap_world, load::WARMUP, load::MEASURED,
                                    Bounds{load::BODIES + load::HEAP_STATICS, 20});
    const Peak scatter =
        perf::measure(scatter_world, load::WARMUP, load::MEASURED, Bounds{load::BODIES, 1});
    // Порога доли у колонны НЕТ, и это не пропуск: она и есть полный перебор, то есть её доля —
    // ровно сто процентов, а потолок в сто процентов совпадает со структурным потолком полного
    // перебора буква в букву. Отдельным `check_share` он был бы вторым именем одной проверки:
    // упасть порознь они не способны. Поломку, ради которой порог заводился (счётчик, считающий
    // пару дважды после переезда `++candidates` под фильтры), ловит здесь `broad_sweep` из
    // `check_bounded`. Поэтому доля оставлена по умолчанию и ниже не утверждается.
    const Peak column =
        perf::measure(column_world, load::WARMUP, load::MEASURED, Bounds{load::BODIES});
    perf::report("heap", heap, load::MEASURED);
    perf::report("scatter", scatter, load::MEASURED);
    perf::report("column", column, load::MEASURED);

    // Константа обязана описывать сцену, а не помнить её прошлую редакцию: `HEAP_STATICS` входит и в
    // ёмкость мира выше, и в полный перебор широкой фазы ниже, а расходится с реальностью молча.
    check(heap_statics == load::HEAP_STATICS, "the heap scene adds exactly HEAP_STATICS statics");

    check(heap.allocs == 0, "heap scene: the settled step allocates nothing");
    check(scatter.allocs == 0, "scatter scene: the settled step allocates nothing");
    check(column.allocs == 0, "column scene: the settled step allocates nothing");

    // Позитивный контроль СЧЁТЧИКА, отдельно от контроля сцен ниже и по другой причине. Три нуля
    // выше держатся на том, что перехват работает, а неработающий перехват даёт ровно те же три
    // нуля — и выглядит самым зелёным гейтом на свете. Замещение операторов действует на ПРОГРАММУ,
    // так что доказательство из соседней цели сюда не переносится: у этого бинаря свой.
    //
    // Выделение зовётся через непрозрачную границу — тело в `framework_alloc_probe_control.cpp`;
    // строкой `new` по месту контроль вакуумен ровно там, где нужен, разбор в шапке заголовка.
    // Выравненной формы здесь нет намеренно: на пути шага над-выравненных типов не бывает, а сама
    // ветка перехвата закреплена в `framework_physics_rt_test.cpp` — второй её экземпляр проверял бы
    // тот же исходник дважды.
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool counter_ok = framework::probe::control::plain_allocation();
    const long counter_control = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(counter_control > 0, "control: the counter sees a real allocation");
    check(counter_ok, "control: that allocation really was handed out and written to");

    // Позитивный контроль сцен: пустая или разъехавшаяся сцена дала бы нули по всем счётчикам и
    // прошла бы любой потолок сверху. Гейт обязан сначала доказать, что работа вообще есть, и что
    // сцена осталась ТОЙ, ради которой заведена.
    check(heap.pairs > load::BODIES / 2, "control: the heap really is in contact");
    check(heap.velocity_projections > 0, "control: the solver really runs on the heap");
    check(scatter.narrow_checks > 0, "control: the scatter really reaches the narrowphase");
    check(scatter.pairs < load::BODIES / 10, "control: the scatter really stays sparse");
    // У колонны контроль обратный по знаку: её тела кинематические, решатель молчит, и все счётчики
    // кроме широкой фазы нулевые сами собой — потолок сверху проходит там вакуумно. Утверждать надо
    // «вырождение всё ещё воспроизводится»: разъедься тела по X от правки раскладки, сцена перестала
    // бы мерить худший случай SAP, оставшись зелёной.
    check(column.broad_candidates >= full_sweep(load::BODIES) * 90 / 100,
          "control: the column really degenerates the sweep to the full pairwise scan");

    check_bounded("heap", heap);
    check_bounded("scatter", scatter);
    check_bounded("column", column);
    check_share("heap", heap);
    check_share("scatter", scatter);

    // Эталон перепечатан вместе с подъёмом `VELOCITY_ITERATIONS` с 8 до 16, и сдвиг у него двойной.
    // Проекции скорости выросли вдвое напрямую (17576 -> 38448) — их число и есть число итераций,
    // помноженное на точки. Остальные счётчики сдвинулись КОСВЕННО: решатель сходится плотнее, куча
    // укладывается иначе, и вместе с раскладкой поехали контакты (1435 -> 1555 пар) и кандидаты
    // широкой фазы (11972 -> 11951). Второе — причина, по которой равенство здесь точное: потолок
    // сверху назвал бы «пар стало больше» законным и молча пропустил бы правку, меняющую сцену.
    check_reference("heap", heap, Reference{500, 1555, 11951, 1555, 38448, 2248});
    check_reference("scatter", scatter, Reference{500, 9, 535, 9, 112, 6});
    check_reference("column", column, Reference{0, 0, 124750, 0, 0, 0});

    std::printf("framework-physics-perf: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
