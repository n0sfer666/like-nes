#include <cstdio>
#include <string>

#include "framework_json_test_run.hpp"

// Строки разборщика: экранирование, суррогаты, проверка UTF-8 и лимит длины. Каждая граница
// таблицы UTF-8 — с обеих сторон: запрет overlong-записей, ошибочно сдвинутый на единицу, иначе
// отбивал бы законный U+0800 или пропускал бы закодированный суррогат.
namespace {

using namespace json_test;

std::string decoded(const std::string& literal) {
    const std::string text = "\"" + literal + "\"";
    Run r;
    if (!parse(r, text) || r.doc[0].kind != json::Kind::String) return "<refused>";
    return std::string(r.doc[0].s);
}

bool refused_string(const std::string& literal, const char* message, uint32_t column) {
    return refused_with("\"" + literal + "\"", message, 1, column);
}

void test_escapes() {
    check(decoded(R"(\"\\\/\b\f\n\r\t)") == "\"\\/\b\f\n\r\t", "short escapes");
    check(decoded(R"(A\u007f)") == "A\x7f", "one-byte code points");
    check(decoded(R"(\u0080\u07ff)") == "\xC2\x80\xDF\xBF", "two-byte boundaries");
    check(decoded(R"(\u0800\uffff)") == "\xE0\xA0\x80\xEF\xBF\xBF", "three-byte boundaries");
    check(decoded(R"(\u00E9\u20aC)") == "\xC3\xA9\xE2\x82\xAC", "mixed-case hex");
    check(decoded(R"(\ud83d\ude00)") == "\xF0\x9F\x98\x80", "a surrogate pair");
    check(decoded(R"(\ud800\udc00)") == "\xF0\x90\x80\x80", "the lowest pair");
    check(decoded(R"(\udbff\udfff)") == "\xF4\x8F\xBF\xBF", "the highest pair");
    check(decoded(R"(a\nb)") == "a\nb", "an escape between plain bytes");

    const std::string plain = "\"plain\"";
    Run p;
    if (parsed(p, plain, "a plain string parses"))
        check(p.doc[0].s.data() == plain.data() + 1 && p.arena.used() == 0,
              "a string without escapes is a slice of the input");
    const std::string esc = "\"a\\tb\"";
    Run e;
    if (parsed(e, esc, "an escaped string parses"))
        check(e.arena.used() > 0 && e.doc[0].s == "a\tb", "an escaped string lives in the arena");

    Run none;
    none.arena.init(0);
    check(!json::parse(bytes(esc), none.arena, none.doc, none.err) &&
              none.err.message == "string arena exhausted",
          "an exhausted arena is a refusal, not a truncation");
    check(json::parse(bytes(plain), none.arena, none.doc, none.err), "plain needs no arena");
}

void test_escape_refusals() {
    check(refused_string(R"(\ud83d)", "unpaired high surrogate in \\u escape", 2), "lone high");
    check(refused_string(R"(\ud83dA)", "unpaired high surrogate in \\u escape", 2),
          "high followed by a non-surrogate");
    check(refused_string(R"(\ud83d\ud83d)", "unpaired high surrogate in \\u escape", 2),
          "high followed by high");
    check(refused_string(R"(\ud83dx)", "unpaired high surrogate in \\u escape", 2),
          "high followed by a plain byte");
    check(refused_string(R"(\udc00)", "unpaired low surrogate in \\u escape", 2) &&
              refused_string(R"(\udfff)", "unpaired low surrogate in \\u escape", 2),
          "lone low at both edges");
    check(refused_string(R"(\ud800)", "unpaired high surrogate in \\u escape", 2) &&
              refused_string(R"(\udbff)", "unpaired high surrogate in \\u escape", 2),
          "lone high at both edges");
    check(decoded(R"(\udbff\udc00)") == "\xF4\x8F\xB0\x80" &&
              decoded(R"(\ud7ff\ue000)") == "\xED\x9F\xBF\xEE\x80\x80",
          "neighbours of the surrogate range are plain code points");
    check(refused_string(R"(x\u12)", "invalid \\u escape: expected four hex digits", 3),
          "short \\u");
    check(refused_string(R"(\u12G4)", "invalid \\u escape: expected four hex digits", 2),
          "non-hex \\u");
    check(refused_string(R"(\u0000)", "\\u0000 is not allowed in strings", 2), "escaped NUL");
    check(refused_string(R"(\x)", "invalid escape in string", 2), "unknown escape");
    check(refused_with("\"\\", "invalid escape in string", 1, 2), "backslash at the end");
    check(refused_with("\"abc", "unterminated string", 1, 1), "unterminated");
    check(refused_string("a\x01", "control character in string, escape it", 3), "raw control");
    check(refused_string("a\tb", "control character in string, escape it", 3), "raw tab");
    check(refused_string("\xD0\xB6x\x01", "control character in string, escape it", 4),
          "column after a two-byte character");
}

void test_utf8() {
    const std::string valid = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    check(decoded(valid) == valid, "raw UTF-8 passes byte for byte");
    const char* accepted[] = {"\xC2\x80", "\xDF\xBF", "\xE0\xA0\x80", "\xED\x9F\xBF",
                              "\xEE\x80\x80", "\xF0\x90\x80\x80", "\xF4\x8F\xBF\xBF"};
    for (const char* s : accepted) check(decoded(s) == s, s);
    const char* refused[] = {"\xC0\x80",     "\xC1\xBF",         "\xE0\x9F\xBF", "\xED\xA0\x80",
                             "\xF0\x8F\xBF\xBF", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80",
                             "\x80",         "\xC3",             "\xE2\x82",     "\xC3\x41"};
    for (const char* s : refused) check(refused_string(s, "invalid UTF-8 in string", 2), s);
}

void test_length() {
    const std::string top(json::MAX_STRING, 'a');
    check(decoded(top) == top, "a string of exactly 1 MiB is accepted");
    check(refused_string(top + "a", "string longer than 1 MiB", 1), "1 MiB + 1 is refused");
    check(refused_string(top.substr(1) + "\\n", "string longer than 1 MiB", 1),
          "an escape that steps over the limit is refused");
    check(refused_with("{\"" + top + "a\": 1}", "string longer than 1 MiB", 1, 2),
          "keys obey the same limit");
}

} // namespace

int main() {
    std::printf("json parser: strings\n");
    test_escapes();
    test_escape_refusals();
    test_utf8();
    test_length();
    return verdict("framework-json-string");
}
