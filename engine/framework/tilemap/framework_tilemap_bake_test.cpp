#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "map_bake.hpp"
#include "map_read.hpp"
#include "platform_args.hpp"

// Карта как испечённые ДАННЫЕ (вертикаль 2 спеки #16): текст исходника -> zero-parse таблица ->
// сетка в рантайме. Гейт спрашивает два разных вопроса, и каждый ловит свой класс поломки:
//   * round-trip — доехало ли ВСЁ написанное, тайл в тайл и поле в поле;
//   * голден байтов — не поехала ли раскладка (round-trip пережил бы согласованную перестановку
//     пекаря и читателя, а `game.bundle` в git — нет: старая игра прочитала бы новую таблицу молча).
//
// ОТКАЗЫ — и пекаря (номер строки), и читателя (порченая секция) — живут в отдельной цели
// `..._refusal_test`, по тому же основанию, что у профиля #16: имя упавшей цели в логе CI обязано
// отличать «раскладка разъехалась» от «порча прошла молча».
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::tilemap;

constexpr uint64_t GOLDEN = 0xcd034f07f57b5f8bull;

// Две карты, а не одна: таблица именованная, и поиск по имени обязан проверяться на наборе, где у
// него есть выбор. Всё, что можно переставить, у них РАЗНОЕ — ширина не равна высоте, начало по X
// не равно началу по Y, размеры тайла отличаются: перестановка любой пары в пекаре или читателе
// меняет числа местами, а не оставляет их равными. Легенды тоже разные — она принадлежит карте.
const char* SOURCE = R"(
# comments and blank lines are part of the grammar, so the fixture carries them
map       | one
tile_size | 16
origin    | 0 | 0
legend    | . | empty
legend    | X | solid
row       | ..X..
row       | .XXX.
row       | XXXXX

map       | two
tile_size | 8
origin    | -32 | 48
legend    | o | empty
legend    | @ | solid
row       | @oo    # the tail after # is dropped
row       | o@o
row       | oo@
row       | @@@
)";

struct Expect {
    const char* name;
    int32_t origin_x_raw;
    int32_t origin_y_raw;
    int32_t tile_size_raw;
    uint32_t width;
    uint32_t height;
    char solid;
    const char* rows[4];
};

// Ожидание написано ОТДЕЛЬНО от фикстуры, а не выведено из неё: сверка карты с самой собой прошла
// бы и при перепутанных осях. Здесь строки лежат сверху вниз, как в исходнике, и `y` растёт ВНИЗ.
const Expect EXPECT[] = {
    {"one", 0, 0, 16 * 65536, 5, 3, 'X', {"..X..", ".XXX.", "XXXXX", nullptr}},
    {"two", -32 * 65536, 48 * 65536, 8 * 65536, 3, 4, '@', {"@oo", "o@o", "oo@", "@@@"}},
};
constexpr uint32_t MAP_COUNT = sizeof(EXPECT) / sizeof(EXPECT[0]);

uint64_t hash_bytes(const std::vector<uint8_t>& b) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint8_t x : b) h = (h ^ x) * 0x100000001b3ull;
    return h;
}

// Число расхождений, а не «совпало/нет»: карта печатается тайлом с координатами, чтобы одна
// перепутанная ось не пряталась за первым же несовпадением.
int diff(const TileGrid& g, const Expect& e, bool loud) {
    int n = 0;
    for (uint32_t y = 0; y < e.height; ++y) {
        for (uint32_t x = 0; x < e.width; ++x) {
            const bool want = e.rows[y][x] == e.solid;
            if (g.solid_at(static_cast<int32_t>(x), static_cast<int32_t>(y)) == want) continue;
            ++n;
            if (loud)
                std::printf("  FAIL: map '%s' tile %u,%u: bundle %d, source %d\n", e.name, x, y,
                            g.solid_at(static_cast<int32_t>(x), static_cast<int32_t>(y)) ? 1 : 0,
                            want ? 1 : 0);
        }
    }
    return n;
}

void test_round_trip(const std::vector<uint8_t>& table) {
    TileMapTable t;
    check(t.open(table.data(), table.size()), "baked table opens");
    if (!t.valid()) return;
    check(t.count() == MAP_COUNT, "every map of the source is in the table");
    if (t.count() != MAP_COUNT) return;

    for (uint32_t i = 0; i < MAP_COUNT; ++i) {
        const Expect& e = EXPECT[i];
        check(std::strcmp(t.name(i), e.name) == 0, "map name survives the bake");
        const std::optional<TileGrid> g = t.build(i);
        check(g.has_value(), "map builds into a grid");
        if (!g.has_value()) continue;
        check(g->width() == e.width && g->height() == e.height, "map keeps its size");
        check(g->tile_size().raw == e.tile_size_raw, "map keeps its tile size");
        check(g->origin().x.raw == e.origin_x_raw && g->origin().y.raw == e.origin_y_raw,
              "map keeps its origin");
        if (g->width() != e.width || g->height() != e.height) continue;
        check(diff(*g, e, /*loud=*/true) == 0, "every tile of the source is in the grid");
    }

    check(t.find(EXPECT[1].name).has_value(), "a map is found by name");
    check(!t.find("absent").has_value(), "unknown name is refused, not defaulted");
    check(!t.build(MAP_COUNT).has_value(), "index past the end is refused");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("tilemap source bake\n");

    std::vector<uint8_t> table;
    MapBakeError err;
    if (!bake_maps(SOURCE, table, err)) {
        std::printf("  FAIL: line %d: %s\n", err.line, err.message.c_str());
        std::printf("framework-tilemap-bake: FAIL\n");
        return 1;
    }
    const uint64_t h = hash_bytes(table);
    std::printf("  table: %zu bytes, hash 0x%016llx\n", table.size(),
                static_cast<unsigned long long>(h));
    check(h == GOLDEN, "byte table matches the golden");
    test_round_trip(table);

    // Позитивный контроль сверки: испорченное ОЖИДАНИЕ обязано быть отбито той же функцией. Без
    // него «расхождений нет» неотличимо от сверки, которая не сравнивает.
    TileMapTable t;
    if (t.open(table.data(), table.size())) {
        const std::optional<TileGrid> g = t.build(0);
        Expect broken = EXPECT[0];
        broken.rows[0] = "..XX.";
        check(g.has_value() && diff(*g, broken, /*loud=*/false) == 1,
              "the comparison can tell layouts apart");
    }

    std::printf("framework-tilemap-bake: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
