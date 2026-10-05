#include <cstdio>
#include <string>
#include <vector>

#include "font_bake.hpp"
#include "font_fixture.hpp"
#include "text_quads.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

bool same(const render::Quad& q, float x, float y, float w, float h, float tx, float tw, uint32_t rgba) {
    return q.x == x && q.y == y && q.w == w && q.h == h && q.tx == tx && q.ty == 0 && q.tw == tw && q.th == 8 &&
           q.rgba == rgba && q.flip == 0;
}

void test_quads() {
    std::vector<uint8_t> bytes;
    std::string error;
    const FontSrc f = font_fixture::small();
    check(bake_fonts({&f, 1}, bytes, error), "fixture bakes");
    FontTable t;
    check(t.open(bytes.data(), bytes.size()), "fixture opens");
    const FontView tiny = t.find("tiny");
    std::vector<GlyphPlace> places(8);
    const TextStats st = layout_text(tiny, "A \xD0\x96\nA", 0, places);
    places.resize(st.placed);
    std::vector<render::Quad> quads(8);
    const TextQuadStats q = text_quads(tiny, places, TextPen{10, 20, 2, 0xff0000ffu}, quads);
    check(q.quads == 3 && q.dropped == 0, "one quad per inked glyph");
    check(same(quads[0], 10, 20, 6, 16, 4, 3, 0xff0000ffu), "the pen offsets and the scale grows the screen rect");
    check(same(quads[1], 26, 20, 10, 16, 8, 5, 0xff0000ffu), "texels stay at one scale: a Cyrillic glyph");
    check(same(quads[2], 10, 36, 6, 16, 4, 3, 0xff0000ffu), "the second line starts a scaled line lower");
    std::vector<render::Quad> two(2);
    const TextQuadStats cut = text_quads(tiny, places, TextPen{}, two);
    check(cut.quads == 2 && cut.dropped == 1, "quads past the room are dropped and counted");
    check(text_quads(tiny, places, TextPen{0, 0, 0, 0xffffffffu}, quads).quads == 0 &&
              text_quads(FontView{}, places, TextPen{}, quads).quads == 0,
          "zero scale and an empty font make no quad");
}

} // namespace

int main() {
    std::printf("text quads: glyph places to backend quads\n");
    test_quads();
    std::printf("framework-text-quads: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
