#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "text_fields.hpp"
#include "utf8_decode.hpp"

// Слой, который до 2026-08-31 лежал в дереве ЧЕТЫРЬМЯ копиями и не имел ни одной собственной цели:
// про trim/split/число из текста спрашивали пекари — пресетов, профиля, атласа и карты, — и каждый
// спрашивал только про те границы, которые встречаются в его исходнике. Отсюда и разъезд: правка
// «"32767.999999" даёт число ПРОТИВОПОЛОЖНОГО знака» доехала руками до трёх копий из четырёх.
//
// Здесь спрашивается ровно про границы, потому что середина диапазона молчит про все дефекты
// разом: и потерянный знак, и заворачивание, и съеденный разделитель дают на "1.5" один ответ.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void test_fields() {
    const std::vector<std::string> f = framework::core::split_fields("  a |b\t| \r| d ");
    check(f.size() == 4, "split keeps every field, including the empty one");
    check(f[0] == "a" && f[1] == "b" && f[2].empty() && f[3] == "d", "each field is trimmed");
    // Строка без разделителя — одно поле, а не ноль: пекари отличают пустую строку от строки с
    // одним значением по РАЗМЕРУ, и «ноль полей» увело бы их в ветку пропуска.
    const std::vector<std::string> one = framework::core::split_fields("solo");
    check(one.size() == 1 && one[0] == "solo", "a line without a bar is one field");
    check(framework::core::trim("\t \r").empty(), "a field of blanks trims to empty");
}

void test_fix() {
    fix32 v = fix32::from_int(7);
    check(framework::core::parse_fix("0", v) && v == fix32::from_raw(0), "zero");
    check(framework::core::parse_fix("-0.5", v) && v == fix32::from_raw(-fix32::ONE / 2), "sign");
    check(framework::core::parse_fix("32767", v) && v == fix32::from_int(32767), "top whole value");
    // Верхняя граница и есть весь смысл цели: целая часть проходит проверку, а округление дроби
    // вверх добавляет единицу и даёт ровно 2^31 — то есть при `true` число обратного знака.
    check(!framework::core::parse_fix("32767.999999", v), "rounding over INT32_MAX is refused");
    check(!framework::core::parse_fix("32768", v), "a whole part past the range is refused");
    check(framework::core::parse_fix("0.0000001", v) && v == fix32::from_raw(0),
          "precision below the Q16.16 step is dropped, not refused");
    check(!framework::core::parse_fix("", v), "empty");
    check(!framework::core::parse_fix("1,5", v), "a comma is not a decimal point");
    check(!framework::core::parse_fix("1.5x", v), "a trailing character refuses the whole field");
    check(!framework::core::parse_fix("-", v), "a lone sign is not a number");
}

void test_integers() {
    uint32_t u = 7;
    check(framework::core::parse_u32("4294967295", u) && u == 0xFFFFFFFFu, "u32 ceiling");
    check(!framework::core::parse_u32("4294967296", u), "u32 overflow is refused");
    check(!framework::core::parse_u32("6.5", u), "a fraction is refused, not rounded");
    check(!framework::core::parse_u32("-1", u), "u32 takes no sign");
    uint16_t w = 7;
    check(framework::core::parse_u16("65535", w) && w == 0xFFFFu, "u16 ceiling");
    check(!framework::core::parse_u16("65536", w), "u16 overflow is refused, not wrapped");
    check(!framework::core::parse_u16("-4", w), "u16 takes no sign: -4 is not 65532");
}

struct Step {
    uint32_t cp;
    std::size_t at;
};

void decodes(std::string_view s, std::vector<Step> want, const char* what) {
    std::vector<Step> got;
    std::size_t at = 0;
    uint32_t cp = 0;
    while (framework::core::utf8_next(s, at, cp)) got.push_back(Step{cp, at});
    bool same = got.size() == want.size();
    for (std::size_t i = 0; same && i < got.size(); ++i) same = got[i].cp == want[i].cp && got[i].at == want[i].at;
    check(same, what);
}

void test_utf8() {
    constexpr uint32_t BAD = framework::core::UTF8_INVALID;
    decodes("A\xD0\x96", {{0x41, 1}, {0x416, 3}}, "ASCII and a two-byte Cyrillic letter");
    decodes("\xE2\x80\x94\xF0\x9F\x98\x80", {{0x2014, 3}, {0x1F600, 7}}, "three- and four-byte sequences");
    decodes("\xF4\x8F\xBF\xBF", {{0x10FFFF, 4}}, "the last code point");
    decodes("\xC0\x80" "A", {{BAD, 1}, {BAD, 2}, {0x41, 3}}, "an overlong NUL steps one byte at a time");
    decodes("\xE0\x80\x80", {{BAD, 1}, {BAD, 2}, {BAD, 3}}, "an overlong three-byte form");
    decodes("\xED\xA0\x80", {{BAD, 1}, {BAD, 2}, {BAD, 3}}, "an encoded surrogate");
    decodes("\xF4\x90\x80\x80", {{BAD, 1}, {BAD, 2}, {BAD, 3}, {BAD, 4}}, "past U+10FFFF");
    decodes("\xD0" "A", {{BAD, 1}, {0x41, 2}}, "a cut sequence does not swallow the next letter");
    decodes("\xD0", {{BAD, 1}}, "a sequence cut by the end of text");
    check(framework::core::utf8_valid("\xD0\x81\xD1\x91") && !framework::core::utf8_valid("\x80"),
          "validity of the whole text");
}

} // namespace

int main() {
    std::printf("shared text layer of the layer's bakers\n");
    test_fields();
    test_fix();
    test_integers();
    test_utf8();
    std::printf("framework-text: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
