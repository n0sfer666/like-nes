#include <cstdint>
#include <cstdio>
#include <string>

#include "framework_json_test_run.hpp"
#include "text_fields.hpp"

// Числа разборщика: грамматика RFC 8259, целые на краях int64 и fix32 по общему с `parse_fix`
// правилу: хвост после седьмого знака дроби отбрасывается, остаток округляется к ближайшему шагу.
// Шаг fix32 проверяется по обе стороны полушага в семи знаках, а отброс хвоста — числом выше
// полушага, которое после отброса оказывается ниже его.
namespace {

using namespace json_test;

bool number(const std::string& text, json::Kind kind, int64_t i, int32_t raw) {
    Run r;
    if (!parse(r, text)) {
        std::printf("  refused %s: %s\n", text.c_str(), r.err.message.c_str());
        return false;
    }
    const json::Node& n = r.doc[0];
    if (n.kind != kind) return false;
    return kind == json::Kind::Int ? n.i == i : n.f.raw == raw;
}

bool fix(const std::string& text, int32_t raw) { return number(text, json::Kind::Fix, 0, raw); }
bool integer(const std::string& text, int64_t i) { return number(text, json::Kind::Int, i, 0); }

void test_integers() {
    check(integer("0", 0) && integer("-0", 0) && integer("42", 42), "small integers");
    check(integer("9223372036854775807", INT64_MAX), "int64 ceiling");
    check(integer("-9223372036854775808", INT64_MIN), "int64 floor");
    check(refused_with("9223372036854775808", "integer out of int64 range", 1, 1),
          "int64 ceiling + 1 is refused");
    check(refused_with("[-9223372036854775809]", "integer out of int64 range", 1, 2),
          "int64 floor - 1 is refused");
}

void test_grammar() {
    check(refused_with("01", "leading zeros are not allowed in numbers", 1, 1), "leading zero");
    check(refused_with("-01", "leading zeros are not allowed in numbers", 1, 1), "signed zero pad");
    check(refused_with("-", "invalid number", 1, 1), "a lone minus");
    check(refused_with("-a", "invalid number", 1, 1), "minus without digits");
    check(refused_with("1.", "expected a digit after '.'", 1, 3), "a bare point");
    check(refused_with(".5", "expected a value", 1, 1), "no whole part");
    check(refused_with("+1", "expected a value", 1, 1), "a leading plus");
    check(refused_with("1e", "expected a digit in the exponent", 1, 3), "an empty exponent");
    check(refused_with("1e+", "expected a digit in the exponent", 1, 4), "a signed empty exponent");
    check(refused_with("[1, 99999.5]", "number out of fix32 range", 1, 5), "range error column");
}

void test_fix() {
    check(fix("1.5", 98304) && fix("-0.5", -32768), "plain fractions");
    check(fix("1e2", 100 << 16) && fix("2.5e+1", 25 << 16), "an exponent makes a fix32");
    check(fix("1E-2", 655), "a negative exponent rounds like parse_fix");
    check(fix("0.0000152587890625", 1), "one fix32 step");
    check(fix("0.0000076", 0), "just under half a step is zero");
    check(fix("0.0000077", 1), "half a step rounds up");
    check(fix("-0.0000077", -1), "half a step keeps its sign");
    check(fix("0.00000763", 0) && fix("1.00000763", 1 << 16),
          "the tail after the seventh digit is dropped before rounding, like parse_fix");
    check(fix("-1.13686837721616e-13", 0), "Qt noise is zero");
    check(fix("1e-999999999999", 0), "a huge negative exponent is zero");
    check(fix("0e999999999999", 0) && fix("0.0e5", 0), "zero with any exponent is zero");
    check(fix("100000.0e-1", 10000 << 16), "the whole part is judged after the shift");
    check(fix("0.00001e5", 1 << 16), "a fraction shifted into the whole part");
    check(fix("3.2767e4", 32767 << 16), "fix32 top whole value through an exponent");
    check(fix("32767.99999", INT32_MAX) && fix("-32767.99999", -INT32_MAX), "fix32 edges");
    check(refused_with("32767.999993", "number out of fix32 range", 1, 1),
          "rounding over INT32_MAX is refused");
    check(refused_with("32768.0", "number out of fix32 range", 1, 1), "32768 is refused");
    check(refused_with("3.2768e4", "number out of fix32 range", 1, 1), "32768 by exponent");
    check(refused_with("-32768.0", "number out of fix32 range", 1, 1),
          "-32768 is refused like parse_fix does");
    check(refused_with("1e999999999999", "number out of fix32 range", 1, 1), "a huge exponent");
    check(refused_with("281474976710656.0", "number out of fix32 range", 1, 1) &&
              refused_with("281474976710657.0", "number out of fix32 range", 1, 1),
          "2^48 and 2^48 + 1 are refused, not wrapped to 0 and 1 by the shift");
}

void test_shared_rule() {
    fix32 v = fix32::from_int(7);
    check(framework::core::fix_from_decimal(false, "0000000000032767", "", 0, v) &&
              v == fix32::from_int(32767),
          "leading zeros do not count as whole digits");
    check(!framework::core::fix_from_decimal(false, "1", "", INT64_MAX, v), "INT64_MAX exponent");
    check(framework::core::fix_from_decimal(false, "1", "", INT64_MIN, v) && v.raw == 0,
          "INT64_MIN exponent");
    check(!framework::core::fix_from_decimal(false, "1a", "", 0, v) &&
              !framework::core::fix_from_decimal(false, "1", "5 ", 0, v),
          "only ASCII digits are a number");
    check(framework::core::fix_from_decimal(true, "32767", "99999", 0, v) && v.raw == -INT32_MAX,
          "negative edge");
    fix32 a, b;
    check(framework::core::parse_fix("12.3456789", a) &&
              framework::core::fix_from_decimal(false, "123456789", "", -7, b) && a == b,
          "parse_fix and the exponent form agree");
}

bool as_fix_raw(const std::string& text, bool want, int32_t raw) {
    Run r;
    if (!parse(r, text)) return false;
    fix32 out = fix32::from_raw(-1);
    const bool ok = json::as_fix(r.doc[0], out);
    return ok == want && (!want || out.raw == raw);
}

void test_as_fix() {
    check(as_fix_raw("32767", true, 32767 << 16) && as_fix_raw("-32767", true, -32767 * 65536),
          "as_fix takes an integer up to +-32767");
    check(as_fix_raw("32768", false, 0) && as_fix_raw("-32768", false, 0), "as_fix refuses +-32768");
    check(as_fix_raw("1.5", true, 98304), "as_fix passes a fix32 through");
    check(as_fix_raw("\"7\"", false, 0) && as_fix_raw("true", false, 0), "as_fix refuses a non-number");
}

} // namespace

int main() {
    std::printf("json parser: numbers\n");
    test_integers();
    test_grammar();
    test_fix();
    test_shared_rule();
    test_as_fix();
    return verdict("framework-json-number");
}
