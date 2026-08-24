#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "hash.hpp"
#include "platform_args.hpp"
#include "profile.hpp"
#include "profile_read.hpp"

// Шов «испечённое доходит до контроллера»: гейт открывает НЕ свою фикстуру, а лежащий в git
// `example_ugly_game/assets/game.bundle`, достаёт из него секцию `movement` и сверяет профиль с
// `default_profile()` поле в поле.
//
// Фикстура здесь не годится в принципе. Round-trip (`..._profile_test`) доказывает, что пекарь и
// читатель согласованы между собой, но ничего не говорит о том, попала ли секция в бандл, тот ли у
// неё guid и совпадает ли `movement.txt` с умолчанием движка. Ровно эти три вопроса и разъезжаются
// молча: бандл лежит готовым, перепекается руками, и правка манифеста без перепекания не падает —
// она просто оставляет в игре вчерашний профиль.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::character;

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";
const char* PROFILE_NAME = "player";
constexpr std::size_t ARENA_CAPACITY = 64u * 1024u;

struct FixField {
    const char* name;
    fix32 MoveProfile::* f;
};

const FixField FIELDS[] = {
    {"max_speed", &MoveProfile::max_speed},
    {"ground_accel", &MoveProfile::ground_accel},
    {"ground_decel", &MoveProfile::ground_decel},
    {"air_accel", &MoveProfile::air_accel},
    {"air_decel", &MoveProfile::air_decel},
    {"gravity_rise", &MoveProfile::gravity_rise},
    {"gravity_fall", &MoveProfile::gravity_fall},
    {"max_fall_speed", &MoveProfile::max_fall_speed},
    {"jump_height", &MoveProfile::jump_height},
    {"min_jump_height", &MoveProfile::min_jump_height},
    {"corner_correction", &MoveProfile::corner_correction},
    {"ground_snap", &MoveProfile::ground_snap},
    {"max_slope", &MoveProfile::max_slope},
};

// Число расхождений, а не «совпало/нет»: список полей печатается целиком, чтобы одна правка
// манифеста не пряталась за первым же несовпадением.
int diff(const MoveProfile& got, const MoveProfile& want, bool loud) {
    int n = 0;
    for (const FixField& f : FIELDS) {
        if (got.*f.f == want.*f.f) continue;
        ++n;
        if (loud)
            std::printf("  FAIL: %s: bundle %.4f, engine %.4f\n", f.name, (got.*f.f).to_double(),
                        (want.*f.f).to_double());
    }
    if (got.coyote_ticks != want.coyote_ticks) {
        ++n;
        if (loud) std::printf("  FAIL: coyote_ticks: bundle %u, engine %u\n", got.coyote_ticks,
                              want.coyote_ticks);
    }
    if (got.buffer_ticks != want.buffer_ticks) {
        ++n;
        if (loud) std::printf("  FAIL: buffer_ticks: bundle %u, engine %u\n", got.buffer_ticks,
                              want.buffer_ticks);
    }
    return n;
}

bool read_section(const std::string& path, std::vector<uint8_t>& out) {
    asset::AssetManager am;
    if (!am.open(path, ARENA_CAPACITY, /*trusted=*/false)) {
        std::printf("  FAIL: bundle not readable: %s\n", path.c_str());
        return false;
    }
    const uint64_t guid = asset::fnv1a("movement", std::strlen("movement"));
    am.request(guid);
    am.sync_point();
    if (!am.is_ready(guid)) {
        std::printf("  FAIL: no 'movement' section in %s, rebake it with assetc --game\n",
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

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("character movement profile in the shipped bundle\n");
    const std::string path = argc > 1 ? argv[1] : DEFAULT_BUNDLE;

    std::vector<uint8_t> section;
    if (!read_section(path, section)) {
        std::printf("framework-character-bundle: FAIL\n");
        return 1;
    }
    std::printf("  section: %zu bytes from %s\n", section.size(), path.c_str());

    ProfileTable t;
    check(t.open(section.data(), section.size()), "baked section opens as a profile table");
    if (!t.valid()) {
        std::printf("framework-character-bundle: FAIL\n");
        return 1;
    }
    check(t.count() >= 1, "the shipped table carries a profile");

    MoveProfile baked{};
    if (!t.find(PROFILE_NAME, baked)) {
        std::printf("  FAIL: profile '%s' missing from the bundle\n", PROFILE_NAME);
        std::printf("framework-character-bundle: FAIL\n");
        return 1;
    }
    const MoveProfile want = default_profile();
    check(diff(baked, want, /*loud=*/true) == 0, "movement.txt agrees with default_profile()");

    // Позитивный контроль сверки: испорченная копия ОЖИДАНИЯ обязана быть отбита той же функцией.
    // Без него «расхождений нет» неотличимо от сверки, которая не сравнивает.
    MoveProfile broken = want;
    broken.jump_height = want.jump_height + fix32::from_raw(1);
    check(diff(baked, broken, /*loud=*/false) == 1, "the comparison can tell profiles apart");

    std::printf("framework-character-bundle: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
