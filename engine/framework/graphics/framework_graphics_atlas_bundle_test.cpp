#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "atlas_bake.hpp"
#include "atlas_read.hpp"
#include "hash.hpp"
#include "platform_args.hpp"

// Шов «испечённое доходит до таблицы регионов»: гейт открывает НЕ свою фикстуру, а лежащий в git
// `example_ugly_game/assets/game.bundle`, достаёт из него секцию `atlas_regions`, разворачивает её
// ЧИТАТЕЛЕМ и сверяет с разбором `atlas.txt` регион за регионом.
//
// Что он ловит ОДИН: дефект ЧТЕНИЯ. Байтовая сверка `assetc --verify-game` сравнивает свежую
// выпечку с бандлом и читателя не запускает ВОВСЕ, поэтому сдвиг блока записей, потеря знака у
// привязки или поиск, возвращающий не тот номер, для неё невидимы.
//
// Чего он НЕ ловит: согласованную перестановку полей в пекаре и читателе — ожидание приходит из
// текста, но стороны формата договорились между собой. Её держит байтовый голден
// `framework_graphics_atlas_test`, и порознь ни один из трёх не полон.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";
const char* DEFAULT_SOURCE = "example_ugly_game/assets/atlas.txt";
constexpr std::size_t ARENA_CAPACITY = 1024u * 1024u;

// Имя ассета — `atlas_regions`, а не `atlas`: под `atlas` в том же бандле лежит САМА ТЕКСТУРА, и
// совпадение guid означало бы, что одна запись молча вытесняет другую.
const char* SECTION = "atlas_regions";

bool read_section(const std::string& path, std::vector<uint8_t>& out) {
    asset::AssetManager am;
    if (!am.open(path, ARENA_CAPACITY, /*trusted=*/false)) {
        std::printf("  FAIL: bundle not readable: %s\n", path.c_str());
        return false;
    }
    const uint64_t guid = asset::fnv1a(SECTION, std::strlen(SECTION));
    am.request(guid);
    am.sync_point();
    if (!am.is_ready(guid)) {
        std::printf("  FAIL: no '%s' section in %s, rebake it with assetc --game\n", SECTION,
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

// Число расхождений по ПОЛЯМ, а не «совпало/нет»: поле называется словом, чтобы перепутанная пара
// не пряталась за первым же несовпадением.
int diff(const AtlasRegion& r, const ParsedRegion& e, bool loud) {
    int n = 0;
    const struct {
        const char* field;
        int32_t got, want;
    } pairs[] = {
        {"x", r.x, e.x},         {"y", r.y, e.y},
        {"w", r.w, e.w},         {"h", r.h, e.h},
        {"pivot.x", r.pivot.x.raw, e.pivot.x.raw}, {"pivot.y", r.pivot.y.raw, e.pivot.y.raw},
    };
    for (const auto& p : pairs) {
        if (p.got == p.want) continue;
        ++n;
        if (loud)
            std::printf("  FAIL: region '%s' %s: bundle %d, source %d\n", e.name.c_str(), p.field,
                        p.got, p.want);
    }
    return n;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("atlas section in the shipped bundle\n");
    const std::string bundle = argc > 1 ? argv[1] : DEFAULT_BUNDLE;
    const std::string source = argc > 2 ? argv[2] : DEFAULT_SOURCE;

    ParsedAtlas want;
    AtlasBakeError err;
    if (!parse_atlas_file(source, want, err)) {
        std::printf("  FAIL: %s: line %d: %s\n", source.c_str(), err.line, err.message.c_str());
        std::printf("framework-graphics-atlas-bundle: FAIL\n");
        return 1;
    }

    std::vector<uint8_t> section;
    if (!read_section(bundle, section)) {
        std::printf("framework-graphics-atlas-bundle: FAIL\n");
        return 1;
    }
    std::printf("  section: %zu bytes from %s\n", section.size(), bundle.c_str());

    AtlasTable t;
    check(t.open(section.data(), section.size()), "baked section opens as an atlas table");
    if (!t.valid()) {
        std::printf("framework-graphics-atlas-bundle: FAIL\n");
        return 1;
    }
    check(t.count() == want.regions.size(), "every region of the source is in the bundle");
    check(t.page_width() == want.page_width && t.page_height() == want.page_height,
          "the bundle keeps the page size of the source");

    for (std::size_t i = 0; i < want.regions.size(); ++i) {
        const ParsedRegion& e = want.regions[i];
        const std::optional<RegionId> id = t.find(e.name.c_str());
        if (!id.has_value()) {
            std::printf("  FAIL: region '%s' missing from the bundle\n", e.name.c_str());
            ++fails;
            continue;
        }
        // Номер, найденный по имени, обязан быть номером ОБЪЯВЛЕНИЯ — именно его держит кадр клипа
        // (`RegionId` в `clip.hpp`), и разъехаться он может только здесь, на живой таблице.
        check(*id == i, "a name in the bundle resolves to its declaration order");
        const std::optional<AtlasRegion> r = t.region(*id);
        check(r.has_value(), "the region reads back from the bundle");
        if (r.has_value()) check(diff(*r, e, /*loud=*/true) == 0, "atlas.txt agrees with the bundle");
    }

    // Позитивный контроль сверки: испорченное ОЖИДАНИЕ обязано быть отбито той же функцией. Без
    // него «расхождений нет» неотличимо от сверки, которая не сравнивает.
    if (!want.regions.empty()) {
        const std::optional<RegionId> id = t.find(want.regions[0].name.c_str());
        ParsedRegion broken = want.regions[0];
        broken.pivot.y = fix32::from_raw(broken.pivot.y.raw + 1);
        check(id.has_value() && diff(*t.region(*id), broken, /*loud=*/false) == 1,
              "the comparison can tell regions apart");
    }

    std::printf("framework-graphics-atlas-bundle: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
