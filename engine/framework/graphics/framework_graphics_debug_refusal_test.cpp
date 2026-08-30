#include <cstdio>
#include <string>
#include <vector>

#include "atlas_bake.hpp"
#include "debug_draw.hpp"
#include "platform_args.hpp"

// Отказы отладочной отрисовки (шаг E вертикали 1 спеки #17). Утверждение у них общее и ровно одно:
// НЕВЫДАННЫЙ КВАД СЧИТАЕТСЯ. Оверлей, тихо обрезающий хвост или молча пропускающий текст, показывает
// неполную картину, а читается она как «этих примитивов в сцене нет» — то есть врёт про то, ради
// чего его и включили. Поэтому каждая фикстура ниже проверяет ДВА числа сразу: сколько квадов
// выдано и сколько не выдано; проверка одного счётчика прошла бы при потере второго.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;
using framework::Vec2;

fix32 fx(int v) { return fix32::from_int(v); }
Vec2 v2(int x, int y) { return {fx(x), fx(y)}; }

std::string atlas_source(bool with_font) {
    std::string s = "atlas | 256 | 64\nregion | solid | 0 | 0 | 4 | 4 | 2 | 2\n";
    if (!with_font) return s;
    for (int d = 0; d < 10; ++d)
        s += "region | digit_" + std::to_string(d) + " | " + std::to_string(d * 8) +
             " | 8 | 8 | 12 | 4 | 6\n";
    for (int l = 0; l < 26; ++l)
        s += std::string("region | letter_") + static_cast<char>('a' + l) + " | " +
             std::to_string(l * 8) + " | 24 | 8 | 12 | 4 | 6\n";
    return s;
}

bool make_atlas(bool with_font, std::vector<uint8_t>& bytes, AtlasTable& table) {
    AtlasBakeError err;
    if (!bake_atlas(atlas_source(with_font), bytes, err)) return false;
    return table.open(bytes.data(), bytes.size());
}

void test_overflow(const DebugGlyphs& glyphs) {
    DebugQuad storage[3];
    DebugDraw d(storage, 3, glyphs);
    d.frame(v2(0, 0), {fx(10), fx(5)}, fx(2), 0xffffffffu);
    check(d.count() == 3 && d.dropped() == 1, "the bar that did not fit is counted, not silent");
    // Счётчик обязан пережить ещё один примитив после переполнения: обнулись он на следующем
    // вызове — а это ровно та реализация, где `dropped_` живёт внутри `frame`, — потеря первой
    // перекладины стала бы невидимой ко второму кадру.
    d.fill(v2(0, 0), {fx(1), fx(1)}, 0xffffffffu);
    check(d.count() == 3 && d.dropped() == 2, "a later primitive adds to the count, not replaces it");
    d.clear();
    check(d.count() == 0 && d.dropped() == 0, "clear resets both counters");
}

void test_degenerate(const DebugGlyphs& glyphs) {
    DebugQuad storage[8];
    DebugDraw d(storage, 8, glyphs);
    d.line(v2(3, 3), v2(3, 3), fx(2), 0xffffffffu);
    check(d.count() == 0 && d.dropped() == 1, "a zero-length line has no direction and is refused");
    d.clear();
    d.line(v2(0, 0), v2(10, 0), fix32{}, 0xffffffffu);
    check(d.count() == 0 && d.dropped() == 1, "a line of zero thickness draws nothing");
    d.clear();
    d.line(v2(0, 0), v2(10, 0), -fx(2), 0xffffffffu);
    check(d.count() == 0 && d.dropped() == 1, "negative thickness is refused, not flipped");
    d.clear();
    d.frame(v2(0, 0), {fx(10), fx(5)}, fix32{}, 0xffffffffu);
    check(d.count() == 0 && d.dropped() == 4, "a frame of zero thickness loses all four bars");
    // А вот вырожденные ПЕРЕКЛАДИНЫ — не потеря: у рамки толщиной в свою высоту угол уже покрыт
    // горизонталями, и считать их невыданными значило бы кричать про целую картинку.
    d.clear();
    d.frame(v2(0, 0), {fx(10), fx(1)}, fx(2), 0xffffffffu);
    check(d.count() == 2 && d.dropped() == 0, "bars that cover nothing new are not a loss");
}

void test_no_font(const AtlasTable& atlas) {
    DebugGlyphs glyphs;
    check(resolve_debug_glyphs(atlas, glyphs), "an atlas without a font still gives the block");
    check(!glyphs.has_text, "an atlas without a font is reported as such, not defaulted to region 0");
    DebugQuad storage[8];
    DebugDraw d(storage, 8, glyphs);
    d.text("AB 1", v2(0, 0), fx(10), 0xffffffffu);
    check(d.count() == 0 && d.dropped() == 3,
          "text without a font counts every glyph it could not draw");
    d.clear();
    d.line(v2(0, 0), v2(10, 0), fx(2), 0xffffffffu);
    check(d.count() == 1 && d.dropped() == 0, "lines still draw on an atlas without a font");
}

void test_no_block() {
    std::vector<uint8_t> bytes;
    AtlasBakeError err;
    check(bake_atlas("atlas | 64 | 64\nregion | ship | 0 | 0 | 8 | 8 | 4 | 4\n", bytes, err),
          "an atlas without the solid block bakes");
    AtlasTable atlas;
    check(atlas.open(bytes.data(), bytes.size()), "it opens");
    DebugGlyphs glyphs;
    check(!resolve_debug_glyphs(atlas, glyphs), "an atlas without the solid block is refused");
    DebugQuad storage[8];
    DebugDraw d(storage, 8, glyphs);
    d.line(v2(0, 0), v2(10, 0), fx(2), 0xffffffffu);
    d.fill(v2(0, 0), {fx(1), fx(1)}, 0xffffffffu);
    d.frame(v2(0, 0), {fx(10), fx(5)}, fx(2), 0xffffffffu);
    check(d.count() == 0 && d.dropped() == 6,
          "without the block every solid primitive is counted as undrawn");
}

void test_no_storage(const DebugGlyphs& glyphs) {
    // Буфер принадлежит вызывающему, поэтому `nullptr` — его законная ошибка, а не наша: она обязана
    // стать счётчиком, а не записью по нулевому адресу.
    DebugDraw d(nullptr, 16, glyphs);
    d.fill(v2(0, 0), {fx(1), fx(1)}, 0xffffffffu);
    check(d.count() == 0 && d.dropped() == 1, "a null buffer counts, it does not write");
    DebugDraw zero(reinterpret_cast<DebugQuad*>(&fails), 0, glyphs);
    zero.fill(v2(0, 0), {fx(1), fx(1)}, 0xffffffffu);
    check(zero.count() == 0 && zero.dropped() == 1, "a buffer of zero capacity counts too");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("debug overlay refusals\n");

    std::vector<uint8_t> full_bytes, bare_bytes;
    AtlasTable full, bare;
    if (!make_atlas(/*with_font=*/true, full_bytes, full) ||
        !make_atlas(/*with_font=*/false, bare_bytes, bare)) {
        std::printf("  FAIL: the fixture atlases did not bake\n");
        std::printf("framework-graphics-debug-refusal: FAIL\n");
        return 1;
    }
    DebugGlyphs glyphs;
    check(resolve_debug_glyphs(full, glyphs) && glyphs.has_text, "the fixture font resolves");

    test_overflow(glyphs);
    test_degenerate(glyphs);
    test_no_font(bare);
    test_no_block();
    test_no_storage(glyphs);

    std::printf("framework-graphics-debug-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
