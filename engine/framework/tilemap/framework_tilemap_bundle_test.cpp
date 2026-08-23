#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "hash.hpp"
#include "map_bake.hpp"
#include "map_read.hpp"
#include "platform_args.hpp"

// Шов «испечённое доходит до сетки»: гейт открывает НЕ свою фикстуру, а лежащий в git
// `example_ugly_game/assets/game.bundle`, достаёт из него секцию `tilemap`, разворачивает её
// ЧИТАТЕЛЕМ в сетку и сверяет с разбором `tilemap.txt` тайл за тайлом.
//
// Что он ловит ОДИН — проверено сломанной реализацией: индексация флагов в читателе, заменённая на
// `x * height + y`, краснит этот гейт и оставляет `assetc --verify-game` зелёным. Иначе и быть не
// может: байтовая сверка сравнивает свежую выпечку с бандлом и читателя не запускает ВОВСЕ, поэтому
// любой дефект чтения — границы, ось Y, сдвиг блока флагов — для неё невидим.
//
// Чего он НЕ ловит: согласованную перестановку полей в пекаре и читателе. Ожидание приходит из
// разбора текста, но стороны формата договорились между собой, так что сетка сходится — и байтовая
// сверка тоже зелёная, ведь бандл перепечён тем же пекарем. Проверено тем же способом: swap
// width/height в обоих оставляет оба гейта зелёными и краснит ТОЛЬКО байтовый голден
// `framework_tilemap_bake_test` (0xcd034f07f57b5f8b → 0x92285df48a21391b) — раскладку держит
// прибитое число, а не сверка.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::tilemap;

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";
const char* DEFAULT_SOURCE = "example_ugly_game/assets/tilemap.txt";
constexpr std::size_t ARENA_CAPACITY = 1024u * 1024u;

bool read_section(const std::string& path, std::vector<uint8_t>& out) {
    asset::AssetManager am;
    if (!am.open(path, ARENA_CAPACITY, /*trusted=*/false)) {
        std::printf("  FAIL: bundle not readable: %s\n", path.c_str());
        return false;
    }
    const uint64_t guid = asset::fnv1a("tilemap", std::strlen("tilemap"));
    am.request(guid);
    am.sync_point();
    if (!am.is_ready(guid)) {
        std::printf("  FAIL: no 'tilemap' section in %s, rebake it with assetc --game\n",
                    path.c_str());
        am.close();
        return false;
    }
    const asset::Loaded a = am.get(guid);
    const auto* b = static_cast<const uint8_t*>(a.data);
    out.assign(b, b + a.size);
    am.close();
    return true;
}

// Число расхождений, а не «совпало/нет»: тайл называется координатами, чтобы перепутанная ось не
// пряталась за первым же несовпадением. Печатается не больше десяти строк — сдвиг на строку даёт
// расхождение в каждом тайле, и лог CI утонул бы в нём.
int diff(const TileGrid& g, const ParsedMap& m, bool loud) {
    int n = 0;
    if (g.width() != m.width || g.height() != m.height) {
        if (loud)
            std::printf("  FAIL: map '%s': bundle %ux%u, source %ux%u\n", m.name.c_str(), g.width(),
                        g.height(), m.width, m.height);
        return 1;
    }
    for (uint32_t y = 0; y < m.height; ++y) {
        for (uint32_t x = 0; x < m.width; ++x) {
            const TileFlags want = m.flags[static_cast<std::size_t>(y) * m.width + x];
            if (g.at(static_cast<int32_t>(x), static_cast<int32_t>(y)) == want) continue;
            ++n;
            if (loud && n <= 10)
                std::printf("  FAIL: map '%s' tile %u,%u: bundle %u, source %u\n", m.name.c_str(),
                            x, y, g.at(static_cast<int32_t>(x), static_cast<int32_t>(y)), want);
        }
    }
    return n;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("tilemap section in the shipped bundle\n");
    const std::string bundle = argc > 1 ? argv[1] : DEFAULT_BUNDLE;
    const std::string source = argc > 2 ? argv[2] : DEFAULT_SOURCE;

    std::vector<ParsedMap> want;
    MapBakeError err;
    if (!parse_maps_file(source, want, err)) {
        std::printf("  FAIL: %s: line %d: %s\n", source.c_str(), err.line, err.message.c_str());
        std::printf("framework-tilemap-bundle: FAIL\n");
        return 1;
    }

    std::vector<uint8_t> section;
    if (!read_section(bundle, section)) {
        std::printf("framework-tilemap-bundle: FAIL\n");
        return 1;
    }
    std::printf("  section: %zu bytes from %s\n", section.size(), bundle.c_str());

    TileMapTable t;
    check(t.open(section.data(), section.size()), "baked section opens as a map table");
    if (!t.valid()) {
        std::printf("framework-tilemap-bundle: FAIL\n");
        return 1;
    }
    check(t.count() == want.size(), "every map of the source is in the bundle");

    for (const ParsedMap& m : want) {
        const std::optional<TileGrid> g = t.find(m.name.c_str());
        if (!g.has_value()) {
            std::printf("  FAIL: map '%s' missing from the bundle\n", m.name.c_str());
            ++fails;
            continue;
        }
        check(g->tile_size() == m.tile_size, "the bundle keeps the tile size of the source");
        check(g->origin() == m.origin, "the bundle keeps the origin of the source");
        check(diff(*g, m, /*loud=*/true) == 0, "tilemap.txt agrees with the shipped bundle");
    }

    // Позитивный контроль сверки: испорченное ОЖИДАНИЕ обязано быть отбито той же функцией. Без
    // него «расхождений нет» неотличимо от сверки, которая не сравнивает.
    if (!want.empty()) {
        const std::optional<TileGrid> g = t.find(want[0].name.c_str());
        ParsedMap broken = want[0];
        broken.flags[0] = static_cast<TileFlags>(broken.flags[0] ^ TILE_SOLID);
        check(g.has_value() && diff(*g, broken, /*loud=*/false) == 1,
              "the comparison can tell layouts apart");
    }

    std::printf("framework-tilemap-bundle: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
