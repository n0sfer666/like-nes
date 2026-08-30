#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "atlas_bake.hpp"
#include "atlas_format.hpp"
#include "atlas_read.hpp"
#include "platform_args.hpp"

// Отказы нарезки атласа — пекаря по НОМЕРУ СТРОКИ и читателя по ПОРЧЕ СЕКЦИИ (шаг D спеки #17).
// Отделено от `..._atlas_test` доводом карт #16: там вопрос «доехало ли написанное», здесь —
// «отбито ли ненаписуемое», и в одном логе второе тонет за первым.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

struct Case {
    const char* what;
    const char* source;
    int line;
    // Кусок сообщения, которым пекарь ОБЯЗАН назвать причину. Без него набор вакуумен наполовину:
    // почти каждая фикстура ниже — ещё и исходник без единого региона, и пекарь, потерявший
    // конкретную проверку, отбил бы её на той же последней строке по другой причине.
    const char* said;
};

const Case CASES[] = {
    {"a region before the page line",
     "region | a | 0 | 0 | 8 | 8 | 0 | 0\natlas | 64 | 32\n", 1, "before the 'atlas' line"},
    {"an unknown key", "atlas | 64 | 32\nregions | a | 0 | 0 | 8 | 8 | 0 | 0\n", 2, "unknown key"},
    {"the page declared twice", "atlas | 64 | 32\natlas | 64 | 32\n", 2, "set twice"},
    {"a page with a zero side", "atlas | 64 | 0\n", 1, "holds no region"},
    {"a page side that is not whole", "atlas | 64.5 | 32\n", 1, "two whole numbers"},
    {"a page line short of a field", "atlas | 64\n", 1, "expected 'atlas"},
    {"a region with a zero side",
     "atlas | 64 | 32\nregion | a | 0 | 0 | 8 | 0 | 0 | 0\n", 2, "draws nothing"},
    {"a region hanging off the page",
     "atlas | 64 | 32\nregion | a | 56 | 0 | 16 | 8 | 0 | 0\n", 2, "hangs off the page"},
    // Свисание РОВНО на перенос шестнадцатибитной суммы: `65535 + 16` в `uint16_t` даёт 15, то есть
    // регион далеко за краем прошёл бы проверку, сложенную в ширине поля.
    {"a region hanging off by exactly the 16-bit wrap",
     "atlas | 64 | 32\nregion | a | 65535 | 0 | 16 | 8 | 0 | 0\n", 2, "hangs off the page"},
    {"a region without a name",
     "atlas | 64 | 32\nregion |  | 0 | 0 | 8 | 8 | 0 | 0\n", 2, "needs a name"},
    {"a name declared twice",
     "atlas | 64 | 32\nregion | a | 0 | 0 | 8 | 8 | 0 | 0\nregion | a | 8 | 0 | 8 | 8 | 0 | 0\n", 3,
     "declared twice"},
    {"a negative coordinate",
     "atlas | 64 | 32\nregion | a | -4 | 0 | 8 | 8 | 0 | 0\n", 2, "four whole numbers"},
    {"a fractional coordinate",
     "atlas | 64 | 32\nregion | a | 0.5 | 0 | 8 | 8 | 0 | 0\n", 2, "four whole numbers"},
    {"a pivot that is not a number",
     "atlas | 64 | 32\nregion | a | 0 | 0 | 8 | 8 | x | 0\n", 2, "two decimal numbers"},
    {"a region line short of a field",
     "atlas | 64 | 32\nregion | a | 0 | 0 | 8 | 8 | 0\n", 2, "expected 'region"},
    {"a source without a page", "# nothing but a comment\n", 1, "no atlas page"},
    // Номер строки у отказа «на конце файла» — последняя СОДЕРЖАТЕЛЬНАЯ, а не счётчик: исходник
    // кончается переводом строки, и счётчик указывал бы на строку за концом файла.
    {"a source without a region", "atlas | 64 | 32\n\n# tail\n", 1, "no region"},
};

// Исходник, который ОБЯЗАН испечься: без него набор отказов зелен вакуумно — пекарь, отвергающий
// всё подряд, прошёл бы каждый случай выше. Отрицательная привязка стоит ЗДЕСЬ: якорь вне картинки
// законен, и пекарь, отбивший её заодно со знаком в координате, обязан упасть тут.
const char* VALID =
    "atlas | 64 | 32\n"
    "region | first  |  0 | 0 | 16 | 8 | 8  | 4\n"
    "region | second | 16 | 0 | 16 | 8 | -2 | 4.5\n";

void test_baker() {
    std::vector<uint8_t> table;
    AtlasBakeError err;
    if (!bake_atlas(VALID, table, err)) {
        std::printf("  FAIL: the valid source was refused at line %d: %s\n", err.line,
                    err.message.c_str());
        ++fails;
    }
    for (const Case& c : CASES) {
        AtlasBakeError e;
        std::vector<uint8_t> out;
        if (bake_atlas(c.source, out, e)) {
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

void poke16(std::vector<uint8_t>& b, std::size_t at, uint16_t v) { std::memcpy(&b[at], &v, 2); }
void poke32(std::vector<uint8_t>& b, std::size_t at, uint32_t v) { std::memcpy(&b[at], &v, 4); }

bool opens(const std::vector<uint8_t>& b) {
    AtlasTable t;
    return t.open(b.data(), b.size());
}

// Порча читателя: каждая правка одна и точечная и обязана закрыть таблицу. Правится КОПИЯ
// исправной таблицы — подделка, сверяемая сама с собой, ничего не доказывает.
void test_reader(const std::vector<uint8_t>& good) {
    check(opens(good), "the baked table opens");

    struct Damage {
        const char* what;
        std::size_t offset;
        bool tail;   // хвост секции адресуется от конца: там лежит терминатор блоба имён
    };
    const Damage DAMAGE[] = {
        {"magic", 0, false},
        {"version", offsetof(AtlasHeader, version), false},
        {"region count", offsetof(AtlasHeader, region_count), false},
        {"regions offset", offsetof(AtlasHeader, regions_offset), false},
        {"strings offset", offsetof(AtlasHeader, strings_offset), false},
        {"total size", offsetof(AtlasHeader, total_size), false},
        {"the terminator of the name blob", 0, true},
    };
    for (const Damage& d : DAMAGE) {
        std::vector<uint8_t> bad = good;
        const std::size_t at = d.tail ? bad.size() - 1 : d.offset;
        bad[at] = static_cast<uint8_t>(bad[at] ^ 0xff);
        if (opens(bad)) {
            std::printf("  FAIL: damaged %s opened as a table\n", d.what);
            ++fails;
        }
    }

    // Правки, которые побитовым переворотом не выражаются: они обязаны попасть В ПОЛЕ осмысленным
    // значением, иначе таблицу закрывает не та проверка, ради которой случай написан.
    const std::size_t row0 = sizeof(AtlasHeader);
    std::vector<uint8_t> zero_side = good;
    poke16(zero_side, row0 + offsetof(AtlasRow, w), 0);
    check(!opens(zero_side), "a region with a zero side is refused by the reader");

    std::vector<uint8_t> off_page = good;
    poke16(off_page, row0 + offsetof(AtlasRow, x), 64);
    check(!opens(off_page), "a region hanging off the page is refused by the reader");

    std::vector<uint8_t> shrunk = good;
    poke16(shrunk, offsetof(AtlasHeader, page_width), 8);
    check(!opens(shrunk), "a page too small for its own regions is refused");

    std::vector<uint8_t> no_page = good;
    poke16(no_page, offsetof(AtlasHeader, page_height), 0);
    check(!opens(no_page), "a page with a zero side is refused by the reader");

    std::vector<uint8_t> stray_name = good;
    poke32(stray_name, row0 + offsetof(AtlasRow, name_offset), 0xffffu);
    check(!opens(stray_name), "a name pointing outside the blob is refused");

    check(!AtlasTable().open(good.data(), good.size() - 1), "a truncated section is refused");
    check(!AtlasTable().open(good.data(), sizeof(AtlasHeader) - 1),
          "a section shorter than the header is refused");
    check(!AtlasTable().open(nullptr, 0), "an absent section is refused");
}

// Потолок `RegionId` — единственное место, где секция собирается РУКАМИ, а не порчей испечённой:
// заголовок с числом регионов 65536 обязан быть законным во ВСЁМ остальном, иначе его закрывает
// проверка границ блоков, а утверждение о потолке остаётся вакуумным. 65536 регионов подсунуть
// пекарю дёшево нельзя (проверка повтора имени квадратична), и половина решения проверяется здесь.
void test_region_ceiling() {
    constexpr uint32_t COUNT = 0x10000u;
    const auto strings = static_cast<uint32_t>(sizeof(AtlasHeader) + COUNT * sizeof(AtlasRow));
    std::vector<uint8_t> b(static_cast<std::size_t>(strings) + 1, 0);
    AtlasHeader h{};
    std::memcpy(h.magic, ATLAS_MAGIC, sizeof(h.magic));
    h.version = ATLAS_VERSION;
    h.region_count = COUNT;
    h.regions_offset = static_cast<uint32_t>(sizeof(AtlasHeader));
    h.strings_offset = strings;
    h.total_size = strings + 1;
    h.page_width = 64;
    h.page_height = 32;
    AtlasRow r{};
    r.w = 8;
    r.h = 8;
    for (uint32_t i = 0; i < COUNT; ++i)
        std::memcpy(&b[sizeof(AtlasHeader) + i * sizeof(AtlasRow)], &r, sizeof(r));
    std::memcpy(b.data(), &h, sizeof(h));
    check(!opens(b), "more regions than a 16-bit RegionId can name is refused");

    // Контроль: ТА ЖЕ секция с законным числом регионов обязана открыться — иначе её закрывает не
    // потолок, а соседняя проверка, и утверждение выше говорит не о том.
    h.region_count = COUNT - 1;
    std::memcpy(b.data(), &h, sizeof(h));
    check(opens(b), "the same section one region short of the ceiling opens");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("atlas bake and read refusals\n");
    test_baker();

    std::vector<uint8_t> good;
    AtlasBakeError err;
    if (!bake_atlas(VALID, good, err)) {
        std::printf("  FAIL: line %d: %s\n", err.line, err.message.c_str());
        std::printf("framework-graphics-atlas-refusal: FAIL\n");
        return 1;
    }
    test_reader(good);
    test_region_ceiling();

    std::printf("framework-graphics-atlas-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
