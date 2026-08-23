#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "map_bake.hpp"
#include "map_format.hpp"
#include "map_read.hpp"
#include "platform_args.hpp"

// Отказы пекаря и читателя — отдельной целью от `..._bake_test` по тому же основанию, что у
// профиля #16: имя упавшей цели в логе CI обязано отличать «раскладка разъехалась» от «порча
// прошла молча».
//
// У пекаря проверяется НОМЕР СТРОКИ, а не только факт отказа: «bad map» ведёт к чтению исходников
// пекаря, а номер ведёт к правке. У читателя проверяется, что порченая секция не открывается: файл
// бывает обрезанным, чужим и просто старым, и «прочиталось молча» тут значит чтение за концом
// отображения.
namespace {

int fails = 0;

using namespace framework::tilemap;

struct Case {
    const char* what;
    const char* source;
    int line;
};

const Case CASES[] = {
    {"a line before the first map", "row | XX\n", 1},
    {"unknown key", "map | a\nspeed | 1\n", 2},
    {"tile_size set twice", "map | a\ntile_size | 16\ntile_size | 16\n", 3},
    {"missing origin", "map | a\ntile_size | 16\nlegend | . | empty\nrow | .\n", 4},
    {"map without a row", "map | a\ntile_size | 16\norigin | 0 | 0\n", 3},
    {"tile size the grid would round", "map | a\ntile_size | 0.00005\n", 2},
    {"glyph declared twice", "map | a\nlegend | . | empty\nlegend | . | solid\n", 3},
    {"unknown flag", "map | a\nlegend | X | sticky\n", 2},
    {"'empty' combined with a flag", "map | a\nlegend | X | empty | solid\n", 2},
    {"a separator where a glyph belongs", "map | a\nlegend | | | solid\n", 2},
    {"glyph missing from the legend",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\nrow | .X.\n", 5},
    {"a row of another width",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\nrow | ..\nrow | ...\n", 6},
    {"map declared twice", "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\n"
                           "row | .\nmap | a\n", 6},
    {"a source without maps", "# only a comment\n", 1},
};

// Исходник, который ОБЯЗАН испечься: без него набор отказов зелен вакуумно — пекарь, отвергающий
// всё подряд, прошёл бы каждый случай выше.
const char* VALID =
    "map | field\ntile_size | 16\norigin | 0 | -32\nlegend | . | empty\nlegend | X | solid\n"
    "row | .X.\nrow | XXX\n";

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void test_baker() {
    std::vector<uint8_t> table;
    MapBakeError err;
    check(bake_maps(VALID, table, err), "the valid source bakes");
    for (const Case& c : CASES) {
        MapBakeError e;
        std::vector<uint8_t> out;
        if (bake_maps(c.source, out, e)) {
            std::printf("  FAIL: %s: baked instead of being refused\n", c.what);
            ++fails;
            continue;
        }
        if (e.line != c.line) {
            std::printf("  FAIL: %s: refused at line %d, expected %d (%s)\n", c.what, e.line,
                        c.line, e.message.c_str());
            ++fails;
        }
        if (e.message.empty()) {
            std::printf("  FAIL: %s: refused without saying why\n", c.what);
            ++fails;
        }
    }
}

// Порча читателя: каждая правка одна и точечная, и каждая обязана закрыть таблицу. Правится КОПИЯ
// исправной таблицы — подделка, сверяемая сама с собой, ничего не доказывает.
void test_reader(const std::vector<uint8_t>& good) {
    TileMapTable t;
    check(t.open(good.data(), good.size()), "the baked table opens");

    struct Damage {
        const char* what;
        std::size_t offset;
        bool tail;   // хвост секции адресуется от конца: там лежит терминатор блоба имён
    };
    const Damage DAMAGE[] = {
        {"magic", 0, false},
        {"version", offsetof(MapHeader, version), false},
        {"map count", offsetof(MapHeader, map_count), false},
        {"maps offset", offsetof(MapHeader, maps_offset), false},
        {"strings offset", offsetof(MapHeader, strings_offset), false},
        {"total size", offsetof(MapHeader, total_size), false},
        {"the terminator of the name blob", 0, true},
    };
    for (const Damage& d : DAMAGE) {
        std::vector<uint8_t> bad = good;
        const std::size_t at = d.tail ? bad.size() - 1 : d.offset;
        bad[at] = static_cast<uint8_t>(bad[at] ^ 0xff);
        TileMapTable broken;
        if (broken.open(bad.data(), bad.size())) {
            std::printf("  FAIL: damaged %s opened as a table\n", d.what);
            ++fails;
        }
    }
    // Обрезанная секция: заголовок обещает больше байт, чем ему дали.
    TileMapTable cut;
    check(!cut.open(good.data(), good.size() - 1), "a truncated section is refused");
    check(!cut.open(good.data(), sizeof(MapHeader) - 1), "a section shorter than the header is refused");
    check(!cut.open(nullptr, 0), "an absent section is refused");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("tilemap bake and read refusals\n");
    test_baker();

    std::vector<uint8_t> good;
    MapBakeError err;
    if (!bake_maps(VALID, good, err)) {
        std::printf("  FAIL: line %d: %s\n", err.line, err.message.c_str());
        std::printf("framework-tilemap-refusal: FAIL\n");
        return 1;
    }
    test_reader(good);

    std::printf("framework-tilemap-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
