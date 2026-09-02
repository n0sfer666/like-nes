#include <cstdio>
#include <string>
#include <vector>

#include "bake.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

struct Case {
    const char* source;
    const char* what;
};

const Case REJECTED[] = {
    {"param | p | scalar | raw | 0\n", "a parameter before any material"},
    {"material | a | s\n", "material row with a missing field"},
    {"material | a | s | glow\n", "a blend mode nobody implements"},
    {"material | a | s | alpha\nmaterial | a | s2 | alpha\n", "two materials with one name"},
    {"material | a | s | alpha\ninstance | i | nope\n", "an instance of a base that is not there"},
    {"material | a | s | alpha\nparam | p | quat | raw | 0\n", "a parameter type nobody has"},
    {"material | a | s | alpha\nparam | p | scalar | furlongs | 0\n", "a unit nobody has"},
    {"material | a | s | alpha\nparam | p | scalar | raw | x\n", "a value that is not a number"},
    {"material | a | s | alpha\nparam | p | scalar | raw | 0.1.2\n", "two decimal points"},
    {"material | a | s | alpha\nparam | p | vec2 | raw | 1\n", "vec2 given one number"},
    {"material | a | s | alpha\nparam | p | color | raw | 1,1,1,1\n"
     "param | q | color | raw | 1,1,1,1\n"
     "param | r | scalar | raw | 1\n",
     "more floats than the instance block holds"},
    {"material | a | s | alpha\nparam | p | scalar | raw | 0\nparam | p | scalar | raw | 1\n",
     "the same parameter twice in one material"},
    {"material | a | s | alpha\nset | p | 1\n", "an override with nothing to override"},
    {"material | a | s | alpha\nparam | p | scalar | raw | 0\ninstance | i | a\n"
     "param | p | scalar | raw | 1\n",
     "an instance redeclaring a parameter of its base"},
    {"material | a | s | alpha\ntex | n | noise | 0\ntex | m | other | 0\n",
     "two textures on one binding"},
    {"material | a | s | alpha\ntex | n | noise | 9\n", "a binding past the slot count"},
    {"material | a | s | alpha\nplease | draw | it | nicely\n", "a row keyword nobody parses"},
    {"# nothing but a comment\n", "a source with no materials at all"},
    {"material |  | s | alpha\n", "a material with an empty name"},
};

void test_rejections() {
    for (const Case& c : REJECTED) {
        std::vector<uint8_t> bytes;
        mat::BakeError err;
        const bool ok = mat::bake_materials(c.source, bytes, err);
        check(!ok, c.what);
        if (!ok) check(!err.message.empty(), "the refusal says why in words");
    }
}

// Позитивный контроль набора: если бы пекарь отвергал ВСЁ подряд, таблица выше была бы зелена и на
// сломанной реализации. Здесь тот же вызов обязан принять исходник, отличающийся от отвергнутых
// одной правкой.
void test_accepted() {
    const char* const good =
        "material | a | s | alpha\n"
        "param | p | scalar | raw | 0\n"
        "tex | n | noise | 0\n"
        "instance | i | a\n"
        "set | p | 1\n";
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    check(mat::bake_materials(good, bytes, err), "the source the rejected ones are edits of");
}

void test_line_numbers() {
    const char* const src =
        "# comment\n"
        "material | a | s | alpha\n"
        "\n"
        "param | p | scalar | raw | nope\n";
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    check(!mat::bake_materials(src, bytes, err), "the broken value is refused");
    check(err.line == 4, "the refusal names the line the value is on, comments and blanks counted");
}

} // namespace

int main() {
    std::printf("material source refusals\n");
    test_rejections();
    test_accepted();
    test_line_numbers();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
