#include <cstdint>
#include <cstdio>

// Подмена глобальных `operator new`/`delete` живёт здесь, а не в измерительном заголовке: её
// контракт — ровно один TU на программу, и владеть им обязан тот, кто программу и составляет.
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "framework_character_perf_probe.hpp"
#include "platform_args.hpp"

// Гейт 7 спеки #16: уровень целевого размера укладывается в бюджет кадра.
//
// Утверждается здесь НЕ время, а РАБОТА — по тому же основанию, что и в гейте 8 физики: счётчики
// тайлового запроса детерминированы и обязаны совпасть на трёх ОС до единицы, а стенные часы
// раннера GitHub гуляют кратно, и красный шаг «тик занял 40 мкс вместо 20» отключили бы как
// флейкующий, унеся с собой весь гейт. Время печатается в лог и попадает в спеку с названием
// машины; сравнение с бюджетом делает владелец на живом железе (правило 18).
//
// Утверждений ТРИ рода. **Равенство карт** — то, ради чего гейт и написан: маленькая карта и
// целевая, отличающиеся площадью в тридцать два раза, обязаны дать один и тот же счётчик и одну и
// ту же траекторию. Это инвариант 4 спеки в наблюдаемой форме. **Эталон** — точные числа, голден:
// ловит расхождение между ОС, между Debug и Release и любой сдвиг поведения тика. **Структурный
// потолок** — работа худшего тика мала ОТНОСИТЕЛЬНО КАРТЫ, и он переживает перепин эталона: перепин
// механический, а потолок сам ужимается вместе с ростом карты и краснеет ровно тогда, когда запрос
// вырождается в обход уровня.
namespace {

using namespace framework;
using namespace framework::character;
using framework::character::perf::Run;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void same(const char* field, uint64_t small_value, uint64_t target_value) {
    if (small_value == target_value) return;
    std::printf("  FAIL: %s: small map says %llu, target map says %llu\n", field,
                static_cast<unsigned long long>(small_value),
                static_cast<unsigned long long>(target_value));
    ++fails;
}

// Эталон — по ФИКСИРОВАННОМУ окну [`WARMUP_TICKS`, `WARMUP_TICKS` + `MEASURED_TICKS`), а не
// асимптотика маршрута: маршрут периодичен, но период его не совпадает с периодом узора, и никакого
// «установившегося» числа у него нет. Равенство точное, а не потолок: потолок сверху назвал бы
// «запросов стало меньше» законным и молча пропустил бы правку, отрезавшую пробу опоры.
struct Reference {
    uint64_t queries = 0;
    uint64_t scanned = 0;
    uint64_t worst_tick_queries = 0;
    uint64_t worst_tick_scanned = 0;
    uint64_t hash = 0;
};

void expect(const char* field, uint64_t got, uint64_t want) {
    if (got == want) return;
    std::printf("  FAIL: %s = %llu, reference says %llu\n", field,
                static_cast<unsigned long long>(got), static_cast<unsigned long long>(want));
    ++fails;
}

void check_reference(const Run& r, const Reference& ref) {
    expect("queries", r.queries, ref.queries);
    expect("scanned", r.scanned, ref.scanned);
    expect("worst tick queries", r.worst_tick_queries, ref.worst_tick_queries);
    expect("worst tick scanned", r.worst_tick_scanned, ref.worst_tick_scanned);
    expect("trajectory hash", r.hash, ref.hash);
}

// Позитивный контроль маршрута. Прогон, простоявший на месте, дал бы стабильные счётчики, ровное
// равенство карт и совпадающий хеш — то есть выглядел бы самым зелёным гейтом на свете, ничего не
// сказав про тик на уровне.
void check_route(const char* name, const Run& r) {
    char what[160];
    const struct {
        const char* claim;
        bool ok;
    } controls[] = {
        {"the route really spends ticks on the ground", r.ground_ticks > 0},
        {"the route really leaves the ground", r.air_ticks > 0},
        {"the route really hits the ceiling ledge", r.ceiling_ticks > 0},
        {"the route really walks a slope", r.slope_ticks > 0},
        {"the route really crosses several copies of the pattern",
         r.max_col - r.min_col > static_cast<int32_t>(perf::PATTERN_W)},
        {"the route stays clear of the small map's left edge",
         r.min_col >= static_cast<int32_t>(perf::SAFE_MARGIN_TILES)},
        {"the route stays clear of the small map's right edge",
         r.max_col <= static_cast<int32_t>(perf::SMALL_W - perf::SAFE_MARGIN_TILES)},
        {"the settled tick allocates nothing", r.allocs == 0},
    };
    for (const auto& c : controls) {
        std::snprintf(what, sizeof(what), "%s: %s", name, c.claim);
        check(c.ok, what);
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character perf gate (%u x %u tiles)\n", perf::TARGET_W, perf::TARGET_H);

    check(perf::pattern_is_rectangular(), "control: every pattern row is PATTERN_W wide");

    const Scene small = perf::make_level(perf::SMALL_W, perf::SMALL_H);
    const Scene target = perf::make_level(perf::TARGET_W, perf::TARGET_H);
    // Контроль самого сравнения: карты обязаны РАЗЛИЧАТЬСЯ площадью, иначе «счётчики совпали»
    // означает лишь то, что дважды измерена одна и та же карта.
    const uint64_t small_tiles = static_cast<uint64_t>(perf::SMALL_W) * perf::SMALL_H;
    const uint64_t target_tiles = static_cast<uint64_t>(perf::TARGET_W) * perf::TARGET_H;
    check(target_tiles == small_tiles * 32,
          "control: the target map really is thirty-two times the small one by area");

    const Run small_run = perf::measure(small, perf::WARMUP_TICKS, perf::MEASURED_TICKS);
    const Run target_run = perf::measure(target, perf::WARMUP_TICKS, perf::MEASURED_TICKS);
    perf::report("small", small, small_run, perf::MEASURED_TICKS);
    perf::report("target", target, target_run, perf::MEASURED_TICKS);

    // Инвариант 4 спеки #16 в наблюдаемой форме. Хеш стоит рядом со счётчиками не для симметрии:
    // равные счётчики при разошедшейся траектории означали бы, что карты меряют РАЗНОЕ движение
    // одинаковой ценой, и совпадение цены было бы совпадением.
    same("queries", small_run.queries, target_run.queries);
    same("scanned", small_run.scanned, target_run.scanned);
    same("worst tick queries", small_run.worst_tick_queries, target_run.worst_tick_queries);
    same("worst tick scanned", small_run.worst_tick_scanned, target_run.worst_tick_scanned);
    same("trajectory hash", small_run.hash, target_run.hash);

    // Структурный потолок: худший тик рассматривает МЕНЬШЕ ПРОМИЛЛЕ целевой карты. Число здесь не
    // из замера, а из карты, и растёт вместе с ней — запрос, выродившийся в обход уровня, пробьёт
    // его на любой раскладке, а сузившееся окно он законно пропустит: это дело эталона.
    check(target_run.worst_tick_scanned * 1000 <= target_tiles,
          "the worst tick scans less than a per-mille of the target map");

    check_route("small", small_run);
    check_route("target", target_run);

    // Позитивный контроль СЧЁТЧИКА аллокаций: неработающий перехват даёт ровно тот же ноль, что и
    // чистый тик, и выглядит зелёным. Замещение операторов действует на ПРОГРАММУ, поэтому
    // доказательство из соседней цели сюда не переносится — у этого бинаря своё.
    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool counter_ok = framework::probe::control::plain_allocation();
    const long counter_control = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(counter_control > 0, "control: the counter sees a real allocation");
    check(counter_ok, "control: that allocation really was handed out and written to");

    // Эталон снят прогоном 2026-08-30 на macOS/AppleClang и обязан совпасть на трёх ОС и в обеих
    // конфигурациях — расхождение Debug/Release в целочисленной арифметике это не погрешность, а UB
    // (инвариант 1 спеки #15).
    check_reference(target_run, Reference{5345, 38794, 20, 156, 0x5b3bcf0fc03ada62ULL});

    std::printf("framework-character-perf: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
