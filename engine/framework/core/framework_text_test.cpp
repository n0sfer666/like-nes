#include <cstdio>
#include <string>
#include <vector>

#include "text_fields.hpp"

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

} // namespace

int main() {
    std::printf("shared text layer of the layer's bakers\n");
    test_fields();
    test_fix();
    test_integers();
    std::printf("framework-text: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
