#include <cstdio>
#include <string>
#include <vector>

#include "font_bake.hpp"
#include "font_fixture.hpp"
#include "font_read.hpp"
#include "text_layout.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

struct Laid {
    TextStats st;
    std::vector<GlyphPlace> places;
};

std::vector<uint8_t> table_bytes() {
    std::vector<uint8_t> out;
    std::string error;
    const FontSrc f = font_fixture::small();
    if (!bake_fonts({&f, 1}, out, error)) std::printf("    bake: %s\n", error.c_str());
    return out;
}

const std::vector<uint8_t> BYTES = table_bytes();

FontView tiny() {
    static FontTable t;
    static const bool opened = t.open(BYTES.data(), BYTES.size());
    return opened ? t.find("tiny") : FontView{};
}

Laid lay(const char* text, uint32_t max_width, std::size_t room = 64) {
    Laid l;
    l.places.resize(room);
    l.st = layout_text(tiny(), text, max_width, l.places);
    l.places.resize(l.st.placed);
    return l;
}

bool at(const Laid& l, std::size_t i, int32_t x, int32_t y, uint32_t cp) {
    return i < l.places.size() && l.places[i].x == x && l.places[i].y == y && l.places[i].glyph != nullptr &&
           l.places[i].glyph->codepoint == cp;
}

void test_line() {
    const Laid l = lay("A \xD0\x96" "A", 0);
    check(l.st.placed == 3 && l.st.lines == 1 && l.st.unknown == 0, "a space advances without a quad");
    check(at(l, 0, 0, 0, 'A') && at(l, 1, 8, 0, 0x416) && at(l, 2, 14, 0, 'A'),
          "pen moves by the advance of each glyph, Cyrillic included");
    check(l.st.width == 17, "width ends at the ink of the last glyph, not its advance");
    check(lay("", 0).st.lines == 0 && lay("", 0).st.width == 0, "an empty text has no line");
    check(lay("A", 0).st.lines == 1, "one letter is one line");
}

void test_newline_and_wrap() {
    const Laid nl = lay("A\nA", 0);
    check(nl.st.lines == 2 && at(nl, 1, 0, 8, 'A'), "a newline starts the next line one line height lower");
    const Laid wrap = lay("AAAA", 11);
    check(wrap.st.lines == 2 && at(wrap, 2, 8, 0, 'A') && at(wrap, 3, 0, 8, 'A'),
          "the glyph whose ink passes the width moves to the next line");
    check(lay("AAA", 11).st.lines == 1, "ink ending exactly at the width stays on the line");
    const Laid narrow = lay("AA", 1);
    check(narrow.st.lines == 2 && at(narrow, 0, 0, 0, 'A') && at(narrow, 1, 0, 8, 'A'),
          "a glyph wider than the width still takes a line of its own, never loops");
}

void test_unknown() {
    const Laid l = lay("B\xD0" "A\xF0\x9F\x98\x80", 0);
    check(l.st.unknown == 3 && l.st.placed == 4, "a missing glyph, a cut sequence and an emoji are counted");
    check(at(l, 0, 0, 0, '?') && at(l, 1, 4, 0, '?') && at(l, 2, 8, 0, 'A') && at(l, 3, 12, 0, '?'),
          "each unknown character is drawn as '?' and the next letter survives");
}

void test_room() {
    const Laid l = lay("AAAA", 0, 2);
    check(l.st.placed == 2 && l.st.dropped == 2 && l.st.width == 15,
          "glyphs past the room are dropped and counted, the layout still measures them");
    check(layout_text(FontView{}, "A", 0, {}).lines == 0, "an empty font view lays out nothing");
}

} // namespace

int main() {
    std::printf("text layout: glyph places of a UTF-8 string\n");
    test_line();
    test_newline_and_wrap();
    test_unknown();
    test_room();
    std::printf("framework-graphics-text-layout: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
