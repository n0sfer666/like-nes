#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "art.hpp"
#include "asset_manager.hpp"
#include "atlas_read.hpp"
#include "hash.hpp"
#include "platform_args.hpp"

// Анти-дрейф: у нарезки игры-образца ДВА источника — `assets/atlas.txt`, испечённый в секцию
// бандла, и `game::set_regions()`, зашитый в код. Пока рендер игры-образца не переехал на слой
// (вертикаль 3 спеки #17), они существуют оба, и разъехаться могут молча: секция сверяется с
// текстом, код — ни с чем, а на экране разъезд виден только глазами и только на том спрайте, по
// которому попали.
//
// Гейт сверяет их ЧИСЛАМИ. Обратный перевод здесь несущий: `game::rgn` отдаёт UV, вдвинутые внутрь
// на полтексела (`0.5 / AW`), поэтому x0 = u0 * AW - 0.5, а x1 = u1 * AW + 0.5. Полтексела —
// приём САМПЛЕРА, и в таблице их нет намеренно: она говорит, где картинка лежит, а не как её
// фильтровать.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using framework::graphics::AtlasRegion;
using framework::graphics::AtlasTable;

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";
const char* SECTION = "atlas_regions";
constexpr std::size_t ARENA_CAPACITY = 1024u * 1024u;
// Допуск в ПИКСЕЛЯХ страницы: UV идут через float, и обратный перевод возвращает не ровно целое.
// Тысячная пикселя ловит сдвиг на пиксель и не ловит округление float — сдвиг на полтексела,
// который гейт как раз и снимает, вдесятеро больше её.
constexpr float TOL = 1e-3f;

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

// Число расхождений по КРАЯМ, а не «совпало/нет»: край называется словом, чтобы перепутанная ось
// не пряталась за первым же несовпадением.
int diff(const AtlasRegion& r, const game::Region& g, float aw, float ah, const char* name,
         bool loud) {
    int n = 0;
    const struct {
        const char* edge;
        float got, want;
    } pairs[] = {
        {"x0", g.u0 * aw - 0.5f, static_cast<float>(r.x)},
        {"y0", g.v0 * ah - 0.5f, static_cast<float>(r.y)},
        {"x1", g.u1 * aw + 0.5f, static_cast<float>(r.x + r.w)},
        {"y1", g.v1 * ah + 0.5f, static_cast<float>(r.y + r.h)},
    };
    for (const auto& p : pairs) {
        if (std::fabs(p.got - p.want) <= TOL) continue;
        ++n;
        if (loud)
            std::printf("  FAIL: region '%s' %s: art.cpp %.3f, bundle %.3f\n", name, p.edge, p.got,
                        p.want);
    }
    return n;
}

// Имя хранится строкой, а не указателем: `strdup` на MSVC устарел и стоил бы предупреждения в
// гейте, где предупреждение есть ошибка.
struct Named {
    std::string name;
    const game::Region* region;
};

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("atlas.txt against game::set_regions()\n");
    const std::string bundle = argc > 1 ? argv[1] : DEFAULT_BUNDLE;

    game::Atlas art;
    game::set_regions(art);

    std::vector<uint8_t> section;
    if (!read_section(bundle, section)) {
        std::printf("game-atlas-regions: FAIL\n");
        return 1;
    }
    AtlasTable t;
    check(t.open(section.data(), section.size()), "the section opens as an atlas table");
    if (!t.valid()) {
        std::printf("game-atlas-regions: FAIL\n");
        return 1;
    }
    check(t.page_width() == art.w && t.page_height() == art.h,
          "atlas.txt and art.cpp agree on the page size");

    std::vector<Named> want{{"ship", &art.ship},       {"star", &art.star},
                            {"enemy", &art.enemy},     {"bullet", &art.bullet},
                            {"hostile", &art.hostile}, {"boss", &art.boss},
                            {"solid", &art.solid}};
    for (int d = 0; d < 10; ++d)
        want.push_back({"digit_" + std::to_string(d), &art.digit[d]});
    for (int l = 0; l < 26; ++l)
        want.push_back({std::string("letter_") + static_cast<char>('a' + l), &art.letter[l]});
    // Число регионов сверяется тоже: таблица, потерявшая букву, прошла бы цикл ниже целиком —
    // спрашивают-то её по именам из кода.
    check(t.count() == want.size(), "the bundle holds exactly the regions art.cpp names");

    const auto aw = static_cast<float>(art.w), ah = static_cast<float>(art.h);
    for (const Named& n : want) {
        const std::optional<framework::graphics::RegionId> id = t.find(n.name.c_str());
        if (!id.has_value()) {
            std::printf("  FAIL: region '%s' of art.cpp is missing from atlas.txt\n",
                        n.name.c_str());
            ++fails;
            continue;
        }
        const std::optional<AtlasRegion> r = t.region(*id);
        check(r.has_value() && diff(*r, *n.region, aw, ah, n.name.c_str(), /*loud=*/true) == 0,
              "art.cpp agrees with atlas.txt");
    }

    // Позитивный контроль сверки: сдвинутый на пиксель регион обязан быть отбит ТЕМ ЖЕ сравнением.
    // Без него «расхождений нет» неотличимо от сверки, которая не сравнивает, — и весь гейт зелен
    // вакуумно ровно с той опечатки, с которой перестал работать обратный перевод.
    const std::optional<framework::graphics::RegionId> id = t.find("ship");
    game::Region moved = art.ship;
    moved.u0 += 1.0f / aw;
    check(id.has_value() && diff(*t.region(*id), moved, aw, ah, "ship", /*loud=*/false) == 1,
          "the comparison can tell a one-pixel shift apart");

    std::printf("game-atlas-regions: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
