#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "profile_bake.hpp"
#include "profile_read.hpp"

// Профиль как испечённые ДАННЫЕ (решение 2 спеки #16): текст манифеста → zero-parse таблица →
// профиль в рантайме. Гейт спрашивает три разных вопроса, и каждый ловит свой класс поломки:
//   * round-trip — доехало ли ВСЁ написанное, поле в поле;
//   * голден байтов — не поехала ли раскладка (round-trip пережил бы согласованную перестановку
//     пекаря и читателя, а бандл в git — нет: старая игра прочитала бы новую таблицу молча).
//
// ОТКАЗЫ — и пекаря (номер строки, имя ключа), и читателя (порченая секция) — живут в отдельной
// цели `..._profile_refusal_test`, по тому же основанию, что у пресетов #14: имя упавшей цели в
// логе CI обязано отличать «раскладка разъехалась» от «порча прошла молча».
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::character;

// Два профиля, а не один: таблица именованная, и поиск по имени обязан проверяться на наборе, где
// у него есть выбор. Значения ВТОРОГО подобраны все разными и по возрастанию полей — перестановка
// любой пары в пекаре или читателе меняет пару чисел местами, а не оставляет их равными.
const char* MANIFEST = R"(
# comments and blank lines are part of the grammar, so the fixture carries them
profile | player
max_speed       | 340
ground_accel    | 2400
ground_decel    | 3200
air_accel       | 1600
air_decel       | 900
gravity_rise    | 1200
gravity_fall    | 2400
max_fall_speed  | 900
jump_height     | 64
min_jump_height | 16
coyote_ticks    | 6
buffer_ticks    | 6
corner_correction | 4
ground_snap       | 8
max_slope         | 1
climb_speed         | 120
ladder_regrab_ticks | 8

profile | heavy
max_speed       | 200      # the tail after # is dropped
ground_accel    | 1000
ground_decel    | 1100
air_accel       | 1200
air_decel       | 1300
gravity_rise    | 1400
gravity_fall    | 1500
max_fall_speed  | 700
jump_height     | 40
min_jump_height | 12.5     # a fractional value: Q16.16 comes from the text, not strtod
coyote_ticks    | 3
buffer_ticks    | 9
corner_correction | 2.5
ground_snap       | 11
max_slope         | 1.75
climb_speed         | 90
ladder_regrab_ticks | 4
)";

// Голден байтовой таблицы. Перепечатывается ОСОЗНАННО и только вместе с `MOVE_VERSION`: раскладка
// пиннута static_assert'ами, а этот хеш пинит ещё и порядок полей и содержимое блоба имён.
constexpr uint64_t GOLDEN = 0xa554e52327f8d33dull;

uint64_t hash_bytes(const std::vector<uint8_t>& b) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint8_t x : b) h = (h ^ x) * 0x100000001b3ull;
    return h;
}

// Поля и ожидаемые значения — ОДИН список в порядке `MoveRow`, а не десять сравнений россыпью:
// поле, добавленное в профиль, обязано попасть и в сверку, и в ожидание одной правкой.
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
    {"climb_speed", &MoveProfile::climb_speed},
};
constexpr std::size_t FIELD_COUNT = sizeof(FIELDS) / sizeof(FIELDS[0]);

struct Expect {
    const char* name;
    double fix[FIELD_COUNT];
    uint32_t coyote;
    uint32_t buffer;
    uint32_t regrab;
};

// Числа стоят в порядке FIELDS и повторяют манифест ГЛАЗАМИ, а не вычислением из него: ожидание,
// посчитанное тем же кодом, что и результат, проверяет только детерминизм этого кода.
const Expect EXPECT[] = {
    {"player", {340, 2400, 3200, 1600, 900, 1200, 2400, 900, 64, 16, 4, 8, 1, 120}, 6, 6, 8},
    {"heavy", {200, 1000, 1100, 1200, 1300, 1400, 1500, 700, 40, 12.5, 2.5, 11, 1.75, 90}, 3, 9, 4},
};
constexpr uint32_t PROFILE_COUNT = sizeof(EXPECT) / sizeof(EXPECT[0]);

// Сверка ПОЛЕ В ПОЛЕ: `memcmp` структуры сравнил бы заодно паддинг, а «совпали имя и высота
// прыжка» пропустило бы перестановку ground/air целиком.
void same(const MoveProfile& got, const Expect& e) {
    for (std::size_t i = 0; i < FIELD_COUNT; ++i) {
        const fix32 want = fix32::from_float(e.fix[i]);
        if (got.*FIELDS[i].f == want) continue;
        std::printf("  FAIL: %s.%s: got %.4f, want %.4f\n", e.name, FIELDS[i].name,
                    (got.*FIELDS[i].f).to_double(), want.to_double());
        ++fails;
    }
    check(got.coyote_ticks == e.coyote, "coyote_ticks");
    check(got.buffer_ticks == e.buffer, "buffer_ticks");
    check(got.ladder_regrab_ticks == e.regrab, "ladder_regrab_ticks");
}

void test_round_trip(const std::vector<uint8_t>& table) {
    ProfileTable t;
    check(t.open(table.data(), table.size()), "table opens");
    check(t.count() == PROFILE_COUNT, "every profile of the manifest is in the table");
    for (uint32_t i = 0; i < PROFILE_COUNT; ++i) {
        MoveProfile p{};
        check(std::strcmp(t.name(i), EXPECT[i].name) == 0, "name at its index");
        if (!t.find(EXPECT[i].name, p)) {
            std::printf("  FAIL: profile '%s' not found by name\n", EXPECT[i].name);
            ++fails;
            continue;
        }
        same(p, EXPECT[i]);
    }

    MoveProfile miss{};
    check(!t.find("absent", miss), "unknown name is refused, not defaulted");
    check(!t.at(PROFILE_COUNT, miss), "index past the end is refused");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("character movement profile bake\n");

    std::vector<uint8_t> table;
    ProfileBakeError err;
    if (!bake_profiles(MANIFEST, table, err)) {
        std::printf("  FAIL: line %d: %s\n", err.line, err.message.c_str());
        std::printf("framework-character-profile: FAIL\n");
        return 1;
    }
    const uint64_t h = hash_bytes(table);
    std::printf("  table: %zu bytes, hash 0x%016llx\n", table.size(),
                static_cast<unsigned long long>(h));
    check(h == GOLDEN, "byte table matches the golden");
    test_round_trip(table);

    std::printf("framework-character-profile: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
