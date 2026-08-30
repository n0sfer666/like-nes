#include <cstdio>
#include <string>
#include <vector>

#include "atlas_bake.hpp"
#include "debug_draw.hpp"
#include "platform_args.hpp"

// Отладочная отрисовка (шаг E вертикали 1 спеки #17): примитивы -> поток квадов. Гейт спрашивает
// два разных вопроса, и каждый ловит своё:
//   * ГЕОМЕТРИЯ — куда встал каждый квад: центр, полуразмеры, направление. Свёртка её не проверяет:
//     она сложила бы перепутанные оси в такое же число, каким бы оно ни было;
//   * ГОЛДЕН потока — совпал ли он на трёх ОС. Числа `fix32`, то есть целые, и расхождение здесь
//     значит расхождение арифметики, а не «другая картинка».
//
// ОТКАЗЫ (переполнение буфера, нулевая длина, атлас без шрифта) живут в отдельной цели
// `..._debug_refusal_test`: имя упавшей цели в логе CI обязано отличать «геометрия поехала» от
// «оверлей молча обрезал хвост».
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

constexpr uint64_t GOLDEN = 0x922ccf9cf4b0aeccull;

fix32 fx(int v) { return fix32::from_int(v); }
Vec2 v2(int x, int y) { return {fx(x), fx(y)}; }

bool same(Vec2 a, Vec2 b) { return a.x.raw == b.x.raw && a.y.raw == b.y.raw; }

// Шрифт оверлея берётся из НАСТОЯЩЕЙ таблицы атласа, а не из выдуманных номеров: шаг E — первый
// потребитель поиска по имени шага D, и подмена таблицы заглушкой сняла бы с гейта ровно тот шов,
// который он обязан держать.
const char* ATLAS =
    "atlas | 256 | 64\n"
    "region | solid | 0 | 0 | 4 | 4 | 2 | 2\n";

std::string atlas_source() {
    std::string s = ATLAS;
    for (int d = 0; d < 10; ++d)
        s += "region | digit_" + std::to_string(d) + " | " + std::to_string(d * 8) +
             " | 8 | 8 | 12 | 4 | 6\n";
    for (int l = 0; l < 26; ++l)
        s += std::string("region | letter_") + static_cast<char>('a' + l) + " | " +
             std::to_string(l * 8) + " | 24 | 8 | 12 | 4 | 6\n";
    return s;
}

uint64_t hash_quads(const DebugQuad* q, uint32_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    const auto mix = [&h](int64_t v) {
        for (int i = 0; i < 8; ++i) h = (h ^ static_cast<uint8_t>(v >> (i * 8))) * 0x100000001b3ull;
    };
    for (uint32_t i = 0; i < n; ++i) {
        mix(q[i].center.x.raw); mix(q[i].center.y.raw);
        mix(q[i].half.x.raw);   mix(q[i].half.y.raw);
        mix(q[i].dir.x.raw);    mix(q[i].dir.y.raw);
        mix(q[i].rgba);         mix(q[i].region);
    }
    return h;
}

void expect(const DebugQuad& q, Vec2 center, Vec2 half, Vec2 dir, const char* what) {
    if (same(q.center, center) && same(q.half, half) && same(q.dir, dir)) return;
    std::printf("  FAIL: %s: center %.2f %.2f, half %.2f %.2f, dir %.3f %.3f\n", what,
                q.center.x.to_double(), q.center.y.to_double(), q.half.x.to_double(),
                q.half.y.to_double(), q.dir.x.to_double(), q.dir.y.to_double());
    ++fails;
}

void test_line(DebugDraw& d, const DebugQuad* q) {
    d.clear();
    d.line(v2(0, 0), v2(10, 0), fx(2), 0xff0000ffu);
    d.line(v2(0, 0), v2(0, 10), fx(2), 0x00ff00ffu);
    d.line(v2(4, 4), v2(-4, -4), fx(1), 0x0000ffffu);
    check(d.count() == 3 && d.dropped() == 0, "three lines give three quads");
    if (d.count() != 3) return;
    // Длина уходит в ПОЛУШИРИНУ, толщина в полувысоту, направление — единичное: поворот отдаётся
    // вектором, и утверждать про него надо тем же вектором, а не углом.
    expect(q[0], v2(5, 0), {fx(5), fx(1)}, v2(1, 0), "line to the right");
    expect(q[1], v2(0, 5), {fx(5), fx(1)}, v2(0, 1), "line down");
    check(q[2].dir.x.raw < 0 && q[2].dir.y.raw < 0, "the diagonal keeps the sign of both axes");
    check(same(q[2].center, v2(0, 0)), "the diagonal is centred on its midpoint");
}

void test_frame(DebugDraw& d, const DebugQuad* q) {
    d.clear();
    d.frame(v2(0, 0), {fx(10), fx(5)}, fx(2), 0xffffffffu);
    check(d.count() == 4 && d.dropped() == 0, "a frame is four bars");
    if (d.count() != 4) return;
    expect(q[0], v2(0, -4), {fx(10), fx(1)}, v2(1, 0), "top bar sits inside the rect");
    expect(q[1], v2(0, 4), {fx(10), fx(1)}, v2(1, 0), "bottom bar sits inside the rect");
    // Угол покрыт ровно один раз: верхняя перекладина занимает y до -3, левая начинается с -3.
    expect(q[2], v2(-9, 0), {fx(1), fx(3)}, v2(1, 0), "left bar is shortened by the thickness");
    expect(q[3], v2(9, 0), {fx(1), fx(3)}, v2(1, 0), "right bar is shortened by the thickness");

    d.clear();
    d.frame(v2(0, 0), {fx(10), fx(1)}, fx(2), 0xffffffffu);
    check(d.count() == 2 && d.dropped() == 0,
          "a frame as thick as it is tall keeps only the two bars that cover it");
}

void test_text(DebugDraw& d, const DebugQuad* q) {
    d.clear();
    d.text("AB 1", v2(0, 0), fx(10), 0xffffffffu);
    check(d.count() == 3 && d.dropped() == 0, "the space advances the cursor and draws nothing");
    if (d.count() != 3) return;
    const Vec2 half{fx(10) * fx(9) / fx(25), fx(10) * fx(21) / fx(40)};
    expect(q[0], v2(0, 0), half, v2(1, 0), "the first glyph is centred on the anchor");
    expect(q[1], v2(10, 0), half, v2(1, 0), "the second glyph is one cell to the right");
    expect(q[2], v2(30, 0), half, v2(1, 0), "the digit after the space keeps its column");
    check(q[0].region != q[1].region && q[1].region != q[2].region,
          "each glyph resolves to its own region");

    d.clear();
    d.text("ab", v2(0, 0), fx(10), 0xffffffffu);
    // Число квадов проверяется ДО чтения региона: буфер между вызовами не чистится, и сравнение
    // с прошлым содержимым прошло бы ровно на той реализации, где строчные не рисуются вовсе.
    check(d.count() == 2, "lower case draws its glyphs");
    if (d.count() != 2) return;
    const RegionId lower = q[0].region;
    d.clear();
    d.text("AB", v2(0, 0), fx(10), 0xffffffffu);
    check(d.count() == 2 && lower == q[0].region,
          "lower case resolves to the same glyph as upper case");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("debug overlay primitives\n");

    std::vector<uint8_t> table;
    AtlasBakeError err;
    if (!bake_atlas(atlas_source(), table, err)) {
        std::printf("  FAIL: line %d: %s\n", err.line, err.message.c_str());
        std::printf("framework-graphics-debug: FAIL\n");
        return 1;
    }
    AtlasTable atlas;
    DebugGlyphs glyphs;
    check(atlas.open(table.data(), table.size()), "the fixture atlas opens");
    check(resolve_debug_glyphs(atlas, glyphs), "the overlay finds its block in the atlas");
    check(glyphs.has_text, "the overlay finds a full font in the atlas");
    // Разрешение проверяется ПОИМЁННО, а не «шрифт нашёлся»: неразрешённый глиф остаётся нулём, то
    // есть ПЕРВЫМ регионом таблицы, и текст рисовался бы чужой картинкой без единого отказа.
    check(glyphs.solid == atlas.find("solid") && glyphs.letter[0] == atlas.find("letter_a") &&
              glyphs.letter[25] == atlas.find("letter_z") && glyphs.digit[7] == atlas.find("digit_7"),
          "every glyph resolves to the region its own name names");

    DebugQuad storage[64];
    DebugDraw d(storage, 64, glyphs);
    test_line(d, storage);
    test_frame(d, storage);
    test_text(d, storage);

    // Голден снимается с одной сцены, собранной из всех трёх примитивов разом: порознь они уже
    // проверены геометрией выше, а свёртка отвечает на другой вопрос — сошлись ли машины.
    d.clear();
    d.frame(v2(-40, -20), {fx(38), fx(18)}, fx(2), 0x30ff30ffu);
    d.line(v2(-78, -38), v2(-2, -2), fx(1), 0xff3030ffu);
    d.fill(v2(0, 30), {fx(60), fx(8)}, 0x202020c0u);
    d.text("HP 42", v2(-56, 30), fx(12), 0xffffffffu);
    const uint64_t h = hash_quads(storage, d.count());
    std::printf("  debug-overlay hash = 0x%016llx (%u quads)\n", static_cast<unsigned long long>(h),
                d.count());
    check(h == GOLDEN, "the quad stream matches the golden");

    std::printf("framework-graphics-debug: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
