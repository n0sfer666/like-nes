#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "art.hpp"
#include "asset_manager.hpp"
#include "atlas_bake.hpp"
#include "atlas_read.hpp"
#include "atlas_regions.hpp"
#include "hash.hpp"
#include "platform_args.hpp"

// Анти-дрейф: у нарезки игры-образца ДВА источника, и вертикаль 3 спеки #17 развела их по путям.
// Бейкнутый путь читает секцию `atlas_regions` по именам (`regions_from_table`), а
// `game::set_regions()` остался ПРОЦЕДУРНОМУ fallback'у, который рисует пиксели сам. Второй
// источник тут неустраним — картинка у него своя, — а разъехаться они могут молча: на экране
// разъезд виден только глазами и только на том спрайте, по которому попали.
//
// Отсюда ДВА утверждения: процедурная нарезка совпадает с таблицей, и бейкнутая идёт ИЗ ТАБЛИЦЫ,
// а не из той же копии в коде. Первое второе не заменяет: пока числа совпадают, путь, читающий
// код вместо таблицы, отвечает то же самое.
//
// Сверка — ЧИСЛАМИ, и обратный перевод в ней несущий: `game::rgn` вдвигает UV внутрь на полтексела
// (`0.5 / AW`), поэтому x0 = u0 * AW - 0.5, а x1 = u1 * AW + 0.5. Полтексела — приём САМПЛЕРА, и в
// таблице их нет намеренно: она говорит, где картинка лежит, а не как её фильтровать.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++fails; }
}

using framework::graphics::AtlasRegion;
using framework::graphics::AtlasTable;

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";
const char* SECTION = "atlas_regions";
constexpr std::size_t ARENA_CAPACITY = 1024u * 1024u;
// Допуск в ПИКСЕЛЯХ страницы: UV идут через float, и обратный перевод возвращает не ровно целое.
// Тысячная пикселя ловит сдвиг на пиксель и не ловит округление float.
constexpr float TOL = 1e-3f;

bool read_section(const std::string& path, std::vector<uint8_t>& out) {
    asset::AssetManager am;
    if (!am.open(path, ARENA_CAPACITY, /*trusted=*/false)) {
        std::printf("  FAIL: bundle not readable: %s\n", path.c_str());
        return false;   // без секции гейту нечего сказать вовсе
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

// Число расхождений по КРАЯМ, а не «совпало/нет»: край назван словом, чтобы перепутанная ось не
// пряталась за первым же несовпадением.
int diff(const AtlasRegion& r, const game::Region& g, float aw, float ah, const char* name,
         bool loud) {
    int n = 0;
    const struct { const char* edge; float got, want; } pairs[] = {
        {"x0", g.u0 * aw - 0.5f, static_cast<float>(r.x)},
        {"y0", g.v0 * ah - 0.5f, static_cast<float>(r.y)},
        {"x1", g.u1 * aw + 0.5f, static_cast<float>(r.x + r.w)},
        {"y1", g.v1 * ah + 0.5f, static_cast<float>(r.y + r.h)},
    };
    for (const auto& p : pairs) {
        if (std::fabs(p.got - p.want) <= TOL) continue;
        ++n;
        if (loud)
            std::printf("  FAIL: region '%s' %s: cut %.3f, bundle %.3f\n", name, p.edge, p.got,
                        p.want);
    }
    return n;
}

// Имя строкой, а не указателем: `strdup` на MSVC устарел и стоил бы предупреждения в гейте, где
// предупреждение есть ошибка.
struct Named { std::string name; const game::Region* region; };

// Один список имён на оба источника (и один ПОРЯДОК: сверка идёт по номеру): второй, написанный
// рядом, разъехался бы с первым — тем самым способом, ради которого этот гейт и заведён.
std::vector<Named> named_regions(const game::Atlas& a) {
    std::vector<Named> v{{"ship", &a.ship},     {"star", &a.star},       {"enemy", &a.enemy},
                         {"bullet", &a.bullet}, {"boss", &a.boss},       {"hostile", &a.hostile},
                         {"solid", &a.solid}};
    for (int d = 0; d < 10; ++d) v.push_back({"digit_" + std::to_string(d), &a.digit[d]});
    for (int l = 0; l < 26; ++l) v.push_back({std::string("letter_") + char('a' + l), &a.letter[l]});
    return v;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("atlas.txt against the two cuts of the sample\n");
    const std::string bundle = argc > 1 ? argv[1] : DEFAULT_BUNDLE;

    game::Atlas art;
    game::set_regions(art);

    std::vector<uint8_t> section;
    AtlasTable t;
    if (!read_section(bundle, section) || !t.open(section.data(), section.size())) {
        std::printf("  FAIL: the section does not open as an atlas table\n");
        std::printf("game-atlas-regions: FAIL\n");
        return 1;
    }
    check(t.page_width() == art.w && t.page_height() == art.h,
          "atlas.txt and art.cpp agree on the page size");

    const std::vector<Named> want = named_regions(art);
    // Число регионов сверяется тоже: таблица, потерявшая букву, прошла бы циклы ниже целиком —
    // спрашивают-то её по именам из кода.
    check(t.count() == want.size(), "the bundle holds exactly the regions the game names");

    const auto aw = static_cast<float>(art.w), ah = static_cast<float>(art.h);
    game::Atlas from_bundle;
    check(game::regions_from_table(t, from_bundle), "the section resolves every name of the game");
    const std::vector<Named> got = named_regions(from_bundle);
    for (std::size_t i = 0; i < got.size(); ++i) {
        const std::optional<framework::graphics::RegionId> id = t.find(got[i].name.c_str());
        if (!id.has_value()) {
            std::printf("  FAIL: region '%s' of the game is missing from atlas.txt\n",
                        got[i].name.c_str());
            ++fails;
            continue;
        }
        // По ВСЕМ регионам, а не по одному: `ship` квадратный, и перепутанные местами ширина с
        // высотой на нём неразличимы.
        check(diff(*t.region(*id), *got[i].region, aw, ah, got[i].name.c_str(), /*loud=*/true) == 0,
              "the baked cut agrees with atlas.txt");
        // Процедурный fallback: обратный перевод тут не нужен — обе стороны уже UV.
        const game::Region& a = *want[i].region;
        const game::Region& b = *got[i].region;
        check(a.u0 == b.u0 && a.v0 == b.v0 && a.u1 == b.u1 && a.v1 == b.v1,
              "the procedural fallback shows the same cut as the baked path");
    }

    // Позитивный контроль сверки: сдвинутый на пиксель регион обязан быть отбит ТЕМ ЖЕ сравнением
    // — иначе «расхождений нет» неотличимо от сверки, которая не сравнивает.
    game::Region moved = art.ship;
    moved.u0 += 1.0f / aw;
    check(diff(*t.region(*t.find("ship")), moved, aw, ah, "ship", /*loud=*/false) == 1,
          "the comparison can tell a one-pixel shift apart");

    // Подмена таблицы: тот же набор имён, корабль сдвинут на восемь пикселей, страница вдвое
    // больше настоящей. Сдвиг ловит реализацию, оставившую `set_regions()` на бейкнутом пути;
    // страница — ту, что делит на зашитые 512x256. Обе прошли бы циклы выше молча.
    const uint16_t pw = static_cast<uint16_t>(t.page_width() * 2);
    const uint16_t ph = static_cast<uint16_t>(t.page_height() * 2);
    std::string text = "atlas | " + std::to_string(pw) + " | " + std::to_string(ph) + "\n";
    for (uint16_t i = 0; i < t.count(); ++i) {
        const AtlasRegion r = *t.region(i);
        const int shift = std::strcmp(t.name(i), "ship") == 0 ? 8 : 0;
        text += "region | " + std::string(t.name(i)) + " | " + std::to_string(r.x + shift) + " | " +
                std::to_string(r.y) + " | " + std::to_string(r.w) + " | " + std::to_string(r.h) +
                " | 0 | 0\n";   // привязка гейту не нужна: он сверяет прямоугольник
    }
    framework::graphics::AtlasBakeError err;
    std::vector<uint8_t> blob;
    check(framework::graphics::bake_atlas(text, blob, err), "the stand-in table bakes");
    AtlasTable stand_in;
    game::Atlas baked;
    check(stand_in.open(blob.data(), blob.size()) && game::regions_from_table(stand_in, baked),
          "the stand-in table resolves every name the game asks for");
    check(std::fabs(baked.ship.u0 * static_cast<float>(pw) - 0.5f - 8.0f) <= TOL,
          "the baked cut follows the table, not the copy in the code");
    check(baked.w == pw && baked.h == ph, "the page size comes from the table too");

    // Отказ ЦЕЛИКОМ, а не по частям: таблица без половины имён оставила бы остальные регионы
    // прежними, то есть смешала бы два источника в один атлас — худший из трёх возможных.
    std::vector<uint8_t> partial;
    const char* one = "atlas | 512 | 256\nregion | ship | 8 | 0 | 4 | 4 | 0 | 0\n";
    check(framework::graphics::bake_atlas(one, partial, err), "the partial table bakes");
    AtlasTable few;
    game::Atlas kept = from_bundle;
    check(few.open(partial.data(), partial.size()) && !game::regions_from_table(few, kept),
          "a table missing names is refused whole");
    check(kept.ship.u0 == from_bundle.ship.u0, "the refused table leaves the cut untouched");

    std::printf("game-atlas-regions: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
