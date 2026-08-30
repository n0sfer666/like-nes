#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "atlas_bake.hpp"
#include "atlas_read.hpp"
#include "platform_args.hpp"

// Нарезка атласа как испечённые ДАННЫЕ (шаг D вертикали 1 спеки #17): текст исходника ->
// zero-parse таблица -> регион по имени в рантайме. Гейт спрашивает два разных вопроса, и каждый
// ловит свой класс поломки:
//   * round-trip — доехало ли ВСЁ написанное, поле в поле;
//   * голден байтов — не поехала ли раскладка (round-trip пережил бы согласованную перестановку
//     пекаря и читателя, а `game.bundle` в git — нет: старая игра прочитала бы новую таблицу молча).
//
// ОТКАЗЫ — и пекаря (номер строки), и читателя (порченая секция) — живут в отдельной цели
// `..._atlas_refusal_test`, по тому же основанию, что у карт #16: имя упавшей цели в логе CI
// обязано отличать «раскладка разъехалась» от «порча прошла молча».
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

// Голден байтов таблицы: три ОС обязаны испечь один и тот же поток. Он отвечает «машины сошлись»,
// а не «сошлись на верном» — за верность отвечает round-trip ниже, за раскладку — static_assert'ы
// формата, и порознь ни один из трёх не полон.
constexpr uint64_t GOLDEN = 0xa9b9b74a0152af52ull;

// Три региона, а не один: таблица именованная, и поиск по имени обязан проверяться на наборе, где
// у него есть выбор. Всё, что можно переставить, у них РАЗНОЕ — x не равен y, ширина не равна
// высоте, привязка не равна половине стороны: перестановка любой пары в пекаре или читателе меняет
// числа местами, а не оставляет их равными. У третьего привязка ОТРИЦАТЕЛЬНАЯ — законная запись
// (якорь вне картинки), на которой видно потерю знака при укладке в раскладку.
const char* SOURCE = R"(
# comments and blank lines are part of the grammar, so the fixture carries them
atlas  | 64 | 32

region | first  |  0 |  4 | 16 |  8 | 8    | 4
region | second | 20 |  1 | 12 | 24 | 6.5  | 3.25   # the tail after # is dropped
region | third  | 48 | 24 | 16 |  8 | -2   | 30.5
)";

struct Expect {
    const char* name;
    uint16_t x, y, w, h;
    int32_t pivot_x_raw, pivot_y_raw;
};

// Ожидание написано ОТДЕЛЬНО от фикстуры, а не выведено из неё: сверка таблицы с самой собой прошла
// бы и при перепутанных полях. Q16.16 записан произведением, чтобы число в тесте читалось тем же,
// что в исходнике.
const Expect EXPECT[] = {
    {"first", 0, 4, 16, 8, 8 * 65536, 4 * 65536},
    {"second", 20, 1, 12, 24, 6 * 65536 + 32768, 3 * 65536 + 16384},
    {"third", 48, 24, 16, 8, -2 * 65536, 30 * 65536 + 32768},
};
constexpr uint16_t REGION_COUNT = sizeof(EXPECT) / sizeof(EXPECT[0]);

uint64_t hash_bytes(const std::vector<uint8_t>& b) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint8_t x : b) h = (h ^ x) * 0x100000001b3ull;
    return h;
}

// Число расхождений по ПОЛЯМ, а не «совпало/нет»: поле называется словом, чтобы перепутанная пара
// не пряталась за первым же несовпадением.
int diff(const AtlasRegion& r, const Expect& e, bool loud) {
    int n = 0;
    const struct {
        const char* field;
        int32_t got, want;
    } pairs[] = {
        {"x", r.x, e.x},         {"y", r.y, e.y},
        {"w", r.w, e.w},         {"h", r.h, e.h},
        {"pivot.x", r.pivot.x.raw, e.pivot_x_raw}, {"pivot.y", r.pivot.y.raw, e.pivot_y_raw},
    };
    for (const auto& p : pairs) {
        if (p.got == p.want) continue;
        ++n;
        if (loud)
            std::printf("  FAIL: region '%s' %s: table %d, source %d\n", e.name, p.field, p.got,
                        p.want);
    }
    return n;
}

void test_round_trip(const std::vector<uint8_t>& table) {
    AtlasTable t;
    check(t.open(table.data(), table.size()), "baked table opens");
    if (!t.valid()) return;
    check(t.count() == REGION_COUNT, "every region of the source is in the table");
    check(t.page_width() == 64 && t.page_height() == 32, "the page size survives the bake");
    if (t.count() != REGION_COUNT) return;

    for (uint16_t i = 0; i < REGION_COUNT; ++i) {
        const Expect& e = EXPECT[i];
        check(std::strcmp(t.name(i), e.name) == 0, "region name survives the bake");
        const std::optional<AtlasRegion> r = t.region(i);
        check(r.has_value(), "region reads back by index");
        if (!r.has_value()) continue;
        check(diff(*r, e, /*loud=*/true) == 0, "every field of the source is in the table");
    }

    // Главное утверждение шага: имя ведёт к НОМЕРУ ОБЪЯВЛЕНИЯ, и именно этот номер клип держит в
    // кадре (`RegionId` в `clip.hpp`). Сортировка внутри пекаря сломала бы ровно это.
    for (uint16_t i = 0; i < REGION_COUNT; ++i) {
        const std::optional<RegionId> id = t.find(EXPECT[i].name);
        check(id.has_value() && *id == i, "a name resolves to its declaration order");
    }
    check(!t.find("absent").has_value(), "unknown name is refused, not defaulted");
    check(!t.find("").has_value(), "the empty name matches nothing, not the first region");
    check(!t.region(REGION_COUNT).has_value(), "index past the end is refused");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("atlas source bake\n");

    std::vector<uint8_t> table;
    AtlasBakeError err;
    if (!bake_atlas(SOURCE, table, err)) {
        std::printf("  FAIL: line %d: %s\n", err.line, err.message.c_str());
        std::printf("framework-graphics-atlas: FAIL\n");
        return 1;
    }
    const uint64_t h = hash_bytes(table);
    std::printf("  atlas table = %zu bytes, hash 0x%016llx\n", table.size(),
                static_cast<unsigned long long>(h));
    check(h == GOLDEN, "byte table matches the golden");
    test_round_trip(table);

    // Позитивный контроль сверки: испорченное ОЖИДАНИЕ обязано быть отбито той же функцией. Без
    // него «расхождений нет» неотличимо от сверки, которая не сравнивает.
    AtlasTable t;
    if (t.open(table.data(), table.size())) {
        const std::optional<AtlasRegion> r = t.region(0);
        Expect broken = EXPECT[0];
        broken.pivot_x_raw += 1;
        check(r.has_value() && diff(*r, broken, /*loud=*/false) == 1,
              "the comparison can tell regions apart");
    }

    std::printf("framework-graphics-atlas: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
