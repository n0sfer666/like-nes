#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "profile_bake.hpp"
#include "profile_format.hpp"
#include "profile_read.hpp"

// Отказы на пути профиля: пекаря — на тексте манифеста, читателя — на байтах секции. Отдельная цель
// от round-trip по тому же основанию, что у пресетов #14: «манифест испёкся молча и разошёлся с
// текстом» и «раскладка таблицы поехала» — разные поломки, и имя упавшей цели в логе CI обязано их
// различать.
//
// Ожидается не «отказ», а НОМЕР СТРОКИ и слова: отказ без места ведёт к чтению исходников пекаря
// вместо правки манифеста, а «bad manifest» на любой из семи причин делает набор нечувствительным
// к тому, какую именно из них поймали.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::character;

// Ключи и их законные значения — по одному на строку, в порядке манифеста. Фикстуры собираются из
// них с ОДНОЙ подменой: так номер ожидаемой строки считается по позиции ключа, а не подбирается
// глазами по тексту фикстуры, и правка списка ключей не разъезжается с ожиданиями.
const char* KEYS[][2] = {
    {"max_speed", "340"},        {"ground_accel", "2400"},   {"ground_decel", "3200"},
    {"air_accel", "1600"},       {"air_decel", "900"},       {"gravity_rise", "1200"},
    {"gravity_fall", "2400"},    {"max_fall_speed", "900"},  {"jump_height", "64"},
    {"min_jump_height", "16"},   {"coyote_ticks", "6"},      {"buffer_ticks", "6"},
    {"corner_correction", "4"},  {"ground_snap", "8"},       {"max_slope", "1"},
    {"climb_speed", "120"},      {"ladder_regrab_ticks", "8"},
};
constexpr int KEY_COUNT = static_cast<int>(sizeof(KEYS) / sizeof(KEYS[0]));

// Строка ключа в собранном манифесте: строка 1 — `profile`, дальше ключи по порядку.
constexpr int key_line(int index) { return index + 2; }
// Закрытие профиля называет ПОСЛЕДНЮЮ содержательную строку, а не строку за концом файла: манифест
// кончается переводом строки, и счётчик строк её бы досчитал. Дописанная строка — следующая за ней.
constexpr int CLOSE_LINE = KEY_COUNT + 1;
constexpr int EXTRA_LINE = KEY_COUNT + 2;

std::string manifest(const char* name, const char* key, const char* value) {
    std::string s = std::string("profile | ") + name + "\n";
    for (int i = 0; i < KEY_COUNT; ++i) {
        const bool patched = key != nullptr && std::strcmp(key, KEYS[i][0]) == 0;
        s += std::string(KEYS[i][0]) + " | " + (patched ? value : KEYS[i][1]) + "\n";
    }
    return s;
}

std::string good() { return manifest("player", nullptr, nullptr); }

struct Case {
    const char* what;
    std::string text;
    int line;
    const char* words;
};

// `loud` выключается только для позитивного контроля самой сверки: там несовпадение ОЖИДАЕТСЯ, и
// печатать его строкой FAIL значило бы класть в лог зелёного прогона слово, по которому CI судит.
bool refuses(const Case& c, bool loud = true) {
    std::vector<uint8_t> table;
    ProfileBakeError err;
    if (bake_profiles(c.text, table, err)) {
        if (loud) std::printf("  FAIL: %s: baked silently\n", c.what);
        return false;
    }
    if (err.line != c.line || err.message.find(c.words) == std::string::npos) {
        if (loud)
            std::printf("  FAIL: %s: line %d '%s', expected line %d about '%s'\n", c.what,
                        err.line, err.message.c_str(), c.line, c.words);
        return false;
    }
    return true;
}

// Порча заголовка секции. Позитивный контроль первым утверждением: та же таблица, но НЕ тронутая,
// обязана открываться — набор «испорченное не открылось» проходит и на читателе, который не
// открывает ничего.
void test_reader_refusals() {
    std::vector<uint8_t> table;
    ProfileBakeError err;
    if (!bake_profiles(good(), table, err)) return;   // отбито позитивным контролем ниже
    // Длина утверждается ДО порчи фикстур, и это не тавтология ради компилятора. Испечённая таблица
    // короче заголовка означала бы, что `bake_profiles` вернул успех на пустоте, а `pop_back`/`back`
    // ниже пошли бы мимо границы — то есть набор «испорченное отвергнуто» ронял бы сам себя порчей
    // памяти, а не ловил чужую. gcc 16 читает это статически и отбивает `-Warray-bounds` прямо на
    // сборке: вывести непустоту из чужого `bake_profiles` ему нечем.
    check(table.size() > sizeof(MoveHeader), "baked table is longer than its header");
    if (table.size() <= sizeof(MoveHeader)) return;

    ProfileTable t;
    check(t.open(table.data(), table.size()), "untouched table is the positive control");

    auto patched = [&table](std::size_t at, uint32_t v) {
        std::vector<uint8_t> bad = table;
        std::memcpy(bad.data() + at, &v, sizeof(v));
        return bad;
    };
    struct Broken { std::vector<uint8_t> bytes; const char* what; };
    const Broken BROKEN[] = {
        {[&table] { auto b = table; b[0] = 'X'; return b; }(), "bad magic refused"},
        {patched(offsetof(MoveHeader, version), MOVE_VERSION + 1), "unknown version refused"},
        // Версия 1 отвергается ПО НОМЕРУ, а не разбирается старой раскладкой (решение владельца
        // 2026-08-24). Записана она отдельным случаем от «версии из будущего», потому что это
        // РЕШЕНИЕ, а не следствие: миграции у таблицы нет, и утверждать это обязан прогон.
        {patched(offsetof(MoveHeader, version), MOVE_VERSION - 1), "the previous version refused"},
        {[&table] { auto b = table; b.pop_back(); return b; }(), "truncated table refused"},
        {[&table] { auto b = table; b.back() = 'x'; return b; }(),
         "name blob without its terminator refused"},
        {patched(offsetof(MoveHeader, profile_count), 0xfffffff0u),
         "row count past the section refused"},
        // Три отказа ниже — про указатель, а не про размер: обнулённый счётчик отдаёт «персонаж не
        // настроен» в рантайм, нулевое смещение кладёт строки поверх заголовка, а невыровненное
        // даёт `reinterpret_cast` мимо границы — SIGBUS на strict-align.
        {patched(offsetof(MoveHeader, profile_count), 0u), "table without rows refused"},
        {patched(offsetof(MoveHeader, profiles_offset), 0u), "rows overlapping the header refused"},
    };
    for (const Broken& b : BROKEN) check(!t.open(b.bytes.data(), b.bytes.size()), b.what);

    // Невыровненность — единственный дефект этой фикстуры, и собрана она поэтому руками: сдвинуть
    // одно `profiles_offset` мало, такую таблицу отбивает проверка размера, и утверждение про
    // выравнивание оказалось бы вакуумным. Здесь строки честно съезжают на два байта ВМЕСТЕ со
    // своим содержимым, а все три смещения в заголовке остаются согласованными.
    std::vector<uint8_t> skewed = table;
    skewed.insert(skewed.begin() + sizeof(MoveHeader), 2, 0);
    auto bump = [&skewed](std::size_t at) {
        uint32_t v = 0;
        std::memcpy(&v, skewed.data() + at, sizeof(v));
        v += 2;
        std::memcpy(skewed.data() + at, &v, sizeof(v));
    };
    bump(offsetof(MoveHeader, profiles_offset));
    bump(offsetof(MoveHeader, strings_offset));
    bump(offsetof(MoveHeader, total_size));
    check(!t.open(skewed.data(), skewed.size()), "misaligned row offset refused");
    check(!t.valid(), "a refused open leaves the table unusable, not half-open");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("character movement profile refusals\n");

    const Case CASES[] = {
        {"value before the first profile", "max_speed | 340\n" + good(), 1, "before the first"},
        {"unknown key", good() + "walk_speed | 10\n", EXTRA_LINE, "unknown key"},
        {"key set twice", good() + "max_speed | 340\n", EXTRA_LINE, "set twice"},
        {"profile declared twice", good() + manifest("player", nullptr, nullptr), EXTRA_LINE,
         "declared twice"},
        {"missing key", "profile | player\nmax_speed | 340\n", 2, "is missing ground_accel"},
        {"line without a value", good() + "max_speed\n", EXTRA_LINE, "expected"},
        {"manifest without profiles", "\n# just a comment\n", 2, "declares no profile"},
        {"value above the ceiling", manifest("player", "max_speed", "9000"), key_line(0),
         "outside the range"},
        {"value that is not a number", manifest("player", "max_speed", "fast"), key_line(0),
         "decimal number"},
        {"fractional window", manifest("player", "coyote_ticks", "6.5"), key_line(10),
         "whole number of ticks"},
        // Округление дроби вверх добавляет целую единицу, и на "32767.999999" сумма выходит ровно
        // 2^31: разбор, считающий её в int32, возвращал бы true с числом ПРОТИВОПОЛОЖНОГО знака.
        // Ожидаются слова про число, а не про диапазон — отказ обязан прийти от разбора.
        {"value that overflows Q16.16", manifest("player", "max_speed", "32767.999999"), key_line(0),
         "decimal number"},
        {"min_jump_height above jump_height", manifest("player", "min_jump_height", "80"), CLOSE_LINE,
         "is above jump_height"},
    };
    for (const Case& c : CASES)
        if (!refuses(c)) ++fails;

    // Позитивный контроль, обе стороны. Первая: манифест БЕЗ подмены обязан испечься — набор, где
    // отвергается всё, проходит и на пекаре, который не печёт ничего. Вторая: сверка сообщения
    // обязана уметь не совпасть — иначе слова в таблице выше были бы украшением.
    std::vector<uint8_t> table;
    ProfileBakeError err;
    if (!bake_profiles(good(), table, err)) {
        std::printf("  FAIL: the untouched manifest was refused: line %d: %s\n", err.line,
                    err.message.c_str());
        ++fails;
    }
    const Case wrong{"matcher", manifest("player", "max_speed", "9000"), key_line(0), "unknown key"};
    if (refuses(wrong, /*loud=*/false)) {
        std::printf("  FAIL: the message check accepts words that are not in the message\n");
        ++fails;
    }
    // Третья: сверка НОМЕРА обязана уметь не совпасть отдельно от слов. Без неё условие выше
    // держалось бы на словах целиком — убери из него `err.line`, и все кейсы остались бы зелёными,
    // хотя номер строки и есть заявленный предмет этого файла.
    const Case off_by_one{"line", manifest("player", "max_speed", "9000"), key_line(0) + 1,
                          "outside the range"};
    if (refuses(off_by_one, /*loud=*/false)) {
        std::printf("  FAIL: the line check accepts a line that is not the one reported\n");
        ++fails;
    }

    test_reader_refusals();

    std::printf("framework-character-profile-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
