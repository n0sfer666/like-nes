#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "font_bake.hpp"
#include "font_fixture.hpp"
#include "font_read.hpp"
#include "section_format.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

std::vector<uint8_t> bake(const std::vector<FontSrc>& fonts) {
    std::vector<uint8_t> out;
    std::string error;
    if (!bake_fonts(fonts, out, error)) std::printf("    bake: %s\n", error.c_str());
    return out;
}

void test_round_trip() {
    FontSrc second = font_fixture::small();
    second.name = "wide";
    second.line_height = 12;
    second.page_h = 12;
    const std::vector<uint8_t> bytes = bake({font_fixture::small(), second});
    FontTable t;
    check(t.open(bytes.data(), bytes.size()) && t.count() == 2, "two fonts open");
    const FontView tiny = t.find("tiny");
    check(tiny.row != nullptr && tiny.row->texture_guid == 0x1234 && tiny.row->line_height == 8 &&
              tiny.row->page_w == 32 && tiny.glyphs.size() == 4,
          "row fields survive the bake");
    check(tiny.glyphs[0].codepoint == ' ' && tiny.glyphs[1].codepoint == '?' && tiny.glyphs[2].codepoint == 'A' &&
              tiny.glyphs[3].codepoint == 0x416,
          "glyphs are sorted by code point");
    const FontGlyph* zhe = font_glyph(tiny, 0x416);
    check(zhe != nullptr && zhe->x == 8 && zhe->w == 5 && zhe->advance == 6, "a Cyrillic glyph is found");
    check(font_glyph(tiny, 'B') == nullptr && font_glyph(tiny, 0x10FFFF) == nullptr && font_glyph({}, '?') == nullptr,
          "a missing glyph is null, also in an empty view");
    check(t.find("wide").row != nullptr && t.find("wide").row->line_height == 12 && t.find("none").row == nullptr,
          "find by name");
}

void bake_refused(FontSrc f, const char* expected, const char* what) {
    std::vector<uint8_t> out;
    std::string error;
    const bool ok = bake_fonts({&f, 1}, out, error);
    if (ok || error.find(expected) == std::string::npos) std::printf("    got: %s\n", ok ? "baked" : error.c_str());
    check(!ok && error.find(expected) != std::string::npos, what);
}

void test_bake_refusals() {
    FontSrc f = font_fixture::small();
    f.glyphs.pop_back();
    bake_refused(f, "has no '?' glyph", "a font without the fallback glyph");
    f = font_fixture::small();
    f.glyphs.push_back({'A', 0, 0, 1, 2});
    bake_refused(f, "glyph U+0041 is declared twice", "a glyph twice");
    f = font_fixture::small();
    f.glyphs[0].x = 28;
    bake_refused(f, "glyph U+0416 has no advance or lies outside its page", "a glyph past the page edge");
    f = font_fixture::small();
    f.glyphs[2].advance = 0;
    bake_refused(f, "has no advance", "a glyph of zero advance");
    f = font_fixture::small();
    f.glyphs.push_back({0x9F, 0, 0, 1, 2});
    bake_refused(f, "glyph U+009F is a control", "a C1 control glyph");
    f = font_fixture::small();
    f.glyphs.push_back({0xD800, 0, 0, 1, 2});
    bake_refused(f, "glyph U+D800 is a control or not a code point", "a surrogate glyph");
    f = font_fixture::small();
    f.line_height = 65;
    bake_refused(f, "line height of 65", "a line taller than 64");
    f = font_fixture::small();
    f.glyphs.clear();
    bake_refused(f, "has 0 glyphs", "a font of no glyph");
    std::vector<uint8_t> out;
    std::string error;
    const std::vector<FontSrc> twice = {font_fixture::small(), font_fixture::small()};
    check(!bake_fonts(twice, out, error) && error == "font 'tiny' is declared twice", "a font name twice");
}

void patched(const std::vector<uint8_t>& bytes, std::size_t at, const void* value, std::size_t size,
             const char* what) {
    std::vector<uint8_t> bad = bytes;
    std::memcpy(bad.data() + at, value, size);
    FontTable r;
    check(!r.open(bad.data(), bad.size()), what);
}

template <class T>
void patched(const std::vector<uint8_t>& bytes, std::size_t at, T value, const char* what) {
    patched(bytes, at, &value, sizeof(value), what);
}

void test_reader_refusals() {
    const std::vector<uint8_t> bytes = bake({font_fixture::small()});
    FontTable t;
    check(t.open(bytes.data(), bytes.size()), "the fixture opens");
    const std::size_t row = sizeof(framework::core::SectionHeader);
    FontRow r{};
    std::memcpy(&r, bytes.data() + row, sizeof(r));
    const std::size_t g = r.glyph_offset;
    patched(bytes, row + offsetof(FontRow, pad0), uint16_t{1}, "a set row pad");
    patched(bytes, row + offsetof(FontRow, pad1), uint32_t{1}, "a set tail pad");
    patched(bytes, row + offsetof(FontRow, line_height), uint16_t{65}, "a line past 64");
    patched(bytes, row + offsetof(FontRow, line_height), uint16_t{9}, "a line taller than the page");
    patched(bytes, row + offsetof(FontRow, glyph_count), uint32_t{0}, "a font of no glyph");
    patched(bytes, row + offsetof(FontRow, name_offset), uint32_t{0}, "an empty font name");
    patched(bytes, g + offsetof(FontGlyph, pad), uint16_t{1}, "a set glyph pad");
    patched(bytes, g + 2 * sizeof(FontGlyph) + offsetof(FontGlyph, codepoint), uint32_t{'?'},
            "the same code point twice");
    patched(bytes, g + offsetof(FontGlyph, codepoint), uint32_t{0x1F}, "a control glyph");
    patched(bytes, g + sizeof(FontGlyph) + offsetof(FontGlyph, codepoint), uint32_t{'>'},
            "no '?' glyph: the reader keeps the fallback promise");
    patched(bytes, g + 3 * sizeof(FontGlyph) + offsetof(FontGlyph, x), uint16_t{28}, "a glyph past the page");
    patched(bytes, g + 2 * sizeof(FontGlyph) + offsetof(FontGlyph, advance), uint8_t{0}, "a glyph of no advance");
    check(!t.open(bytes.data(), bytes.size() - 1) && t.count() == 0 && t.find("tiny").row == nullptr,
          "a cut table is refused and leaves the table empty");
}

void test_overlap() {
    FontSrc other = font_fixture::small();
    other.name = "other";
    const std::vector<uint8_t> two = bake({font_fixture::small(), other});
    FontRow first{};
    std::memcpy(&first, two.data() + sizeof(framework::core::SectionHeader), sizeof(first));
    patched(two, sizeof(framework::core::SectionHeader) + sizeof(FontRow) + offsetof(FontRow, glyph_offset),
            first.glyph_offset, "the second font reusing the glyphs of the first");
}

} // namespace

int main() {
    std::printf("fonts: section bake and reader\n");
    test_round_trip();
    test_bake_refusals();
    test_reader_refusals();
    test_overlap();
    std::printf("framework-graphics-font: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
