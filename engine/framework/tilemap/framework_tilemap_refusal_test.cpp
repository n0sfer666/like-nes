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
    // Кусок сообщения, которым пекарь ОБЯЗАН назвать причину. Без него набор вакуумен наполовину:
    // почти каждый исходник ниже неполон и как карта, поэтому закрытие карты отвергло бы его на той
    // же последней строке — и «отказано номером строки» сбывалось бы у пекаря, потерявшего саму
    // проверку. Найдено сломанной реализацией: `slope_words > 9` оставляет набор зелёным, пока
    // сверяется один номер.
    const char* said;
};

const Case CASES[] = {
    {"a line before the first map", "row | XX\n", 1, "before the first"},
    {"unknown key", "map | a\nspeed | 1\n", 2, "unknown key"},
    {"tile_size set twice", "map | a\ntile_size | 16\ntile_size | 16\n", 3, "set twice"},
    {"missing origin", "map | a\ntile_size | 16\nlegend | . | empty\nrow | .\n", 4,
     "is missing origin"},
    {"map without a row", "map | a\ntile_size | 16\norigin | 0 | 0\n", 3, "has no row"},
    {"tile size the grid would round", "map | a\ntile_size | 0.00005\n", 2, "at least 2 raw"},
    {"glyph declared twice", "map | a\nlegend | . | empty\nlegend | . | solid\n", 3,
     "is declared twice"},
    {"unknown flag", "map | a\nlegend | X | sticky\n", 2, "unknown tile flag"},
    {"'empty' combined with a flag", "map | a\nlegend | X | empty | solid\n", 2,
     "cannot be combined"},
    {"a separator where a glyph belongs", "map | a\nlegend | | | solid\n", 2, "one visible glyph"},
    {"glyph missing from the legend",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\nrow | .X.\n", 5,
     "is not in the legend"},
    {"a row of another width",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\nrow | ..\nrow | ...\n", 6,
     "not as wide"},
    {"map declared twice", "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\n"
                           "row | .\nmap | a\n", 6, "is declared twice"},
    {"a source without maps", "# only a comment\n", 1, "declares no map"},
    // Склон: обе проверки совместимости слов. Первая — про то, что биты зеркал СКЛАДЫВАЮТСЯ, то
    // есть два слова дают молча третью ориентацию; вторая — про то, что склон это форма, а не тело,
    // и тайл без телесного флага выпал бы из запроса дыркой в полу, а не отказом. Карта тут полная
    // нарочно: неполной хватило бы закрытия карты, чтобы отказ случился и без этих проверок.
    {"two slope orientations at once",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\n"
     "legend | X | solid | slope_br | slope_tl\nrow | .X\n", 5, "one slope orientation"},
    {"a slope without a body flag",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\nlegend | X | slope_br\n"
     "row | .X\n", 5, "needs a body flag"},
    // Односторонний тайл — те же две проверки по своим основаниям. Тело ему нужно затем же, зачем
    // склону: `oneway` это ПРАВИЛО ГРАНИ, а не тело, и без `solid` держать нечему. Пара со склоном
    // отвергается потому, что держащая грань склона — гипотенуза: правило «пришёл сверху» на ней не
    // выражается вовсе, и молча получилась бы третья геометрия вместо отказа.
    {"a one-way tile without a body flag",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\nlegend | X | oneway\n"
     "row | .X\n", 5, "needs a body flag"},
    {"a one-way slope",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\n"
     "legend | X | solid | slope_br | oneway\nrow | .X\n", 5, "cannot be one-way"},
    // Лестница — снова две, и обе про то, что бит остался бы без потребителя. Со склоном лазание
    // нечем мерить: вертикали у гипотенузы нет. Со сплошным телом без односторонности внутрь тайла
    // нечем влезть, и карта читалась бы как лестница, работая стеной.
    {"a ladder slope",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\n"
     "legend | X | solid | slope_br | ladder\nrow | .X\n", 5, "cannot be a ladder"},
    {"a solid ladder that is not one-way",
     "map | a\ntile_size | 16\norigin | 0 | 0\nlegend | . | empty\n"
     "legend | X | solid | ladder\nrow | .X\n", 5, "must be one-way"},
};

// Исходник, который ОБЯЗАН испечься: без него набор отказов зелен вакуумно — пекарь, отвергающий
// всё подряд, прошёл бы каждый случай выше.
// Обе законные записи лестницы стоят ЗДЕСЬ, а не отдельным случаем: отказы выше утверждают, чего
// пекарь не принимает, и сами по себе сбылись бы и у пекаря, не знающего слова `ladder` вовсе.
const char* VALID =
    "map | field\ntile_size | 16\norigin | 0 | -32\nlegend | . | empty\nlegend | X | solid\n"
    "legend | H | ladder\nlegend | T | solid | oneway | ladder\n"
    "row | .T.\nrow | .H.\nrow | XXX\n";

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
        if (e.message.find(c.said) == std::string::npos) {
            std::printf("  FAIL: %s: refused saying '%s', expected to name '%s'\n", c.what,
                        e.message.c_str(), c.said);
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
