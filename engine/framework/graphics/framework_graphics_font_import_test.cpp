#include <bit>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "font_import.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

constexpr int LINE = 5;

struct Src {
    uint32_t cp;
    const char* key;
    std::vector<uint32_t> rows;
};

std::vector<Src> sources() {
    std::vector<Src> out;
    static std::string keys[33];
    for (uint32_t cp = 0x21; cp <= 0x41; ++cp) {
        keys[cp - 0x21] = std::string(1, static_cast<char>(cp));
        std::vector<uint32_t> rows;
        for (uint32_t r = 0; r < LINE; ++r) rows.push_back((cp * 7 + r * 3) & 0x1F);
        out.push_back(Src{cp, keys[cp - 0x21].c_str(), rows});
    }
    out.push_back(Src{0x416, "\xD0\x96", {0x41, 0x2A, 0x1C, 0x2A, 0x41}});
    out.push_back(Src{' ', " ", {0, 0, 0, 0, 0}});
    return out;
}

std::string json_of(const std::vector<Src>& src) {
    std::string j = "{";
    for (std::size_t i = src.size(); i-- > 0;) {
        const std::string key = src[i].key;
        j += "\"" + (key == "\"" ? "\\\"" : key) + "\": [";
        for (std::size_t r = 0; r < src[i].rows.size(); ++r) j += (r ? ", " : "") + std::to_string(src[i].rows[r]);
        j += i ? "],\n" : "]\n";
    }
    return j + "}";
}

std::span<const std::byte> bytes_of(const std::string& s) { return std::as_bytes(std::span(s.data(), s.size())); }

bool import(const std::string& json, FontAtlas& out, std::string& error) {
    return import_bitmask_font("mono", "mono.json", bytes_of(json), out, error);
}

const FontGlyphSrc* find(const FontAtlas& a, uint32_t cp) {
    for (const FontGlyphSrc& g : a.font.glyphs)
        if (g.codepoint == cp) return &g;
    return nullptr;
}

bool texels_match(const FontAtlas& a, const Src& s, uint32_t& lit) {
    const FontGlyphSrc* g = find(a, s.cp);
    if (g == nullptr) return false;
    for (uint32_t y = 0; y < LINE; ++y)
        for (uint32_t x = 0; x < 7; ++x) {
            const std::size_t at = ((std::size_t{g->y} + y) * a.font.page_w + g->x + x) * 4;
            const bool want = ((s.rows[y] >> x) & 1u) != 0;
            for (std::size_t c = 0; c < 4; ++c)
                if (a.rgba[at + c] != (want ? 255 : 0)) return false;
            lit += want ? 1 : 0;
        }
    return true;
}

void test_oracle() {
    const std::vector<Src> src = sources();
    FontAtlas a;
    std::string error;
    check(import(json_of(src), a, error), "the bitmask font imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    check(a.font.name == "mono" && a.font.line_height == LINE && a.font.glyphs.size() == src.size(),
          "one glyph per key, line height from the rows");
    check(a.font.page_w == 32 * 7 && a.font.page_h == 2 * LINE && a.rgba.size() == 32u * 7 * 2 * LINE * 4,
          "35 glyphs of cell 7 make two rows of 32 cells");
    uint32_t lit = 0, want = 0;
    bool every = true;
    for (const Src& s : src) {
        every = every && texels_match(a, s, lit);
        for (uint32_t r : s.rows) want += static_cast<uint32_t>(std::popcount(r));
    }
    check(every, "every texel of every cell is white where its mask bit is set and clear elsewhere");
    uint32_t white = 0;
    for (std::size_t i = 0; i < a.rgba.size(); i += 4) white += a.rgba[i + 3] == 255 ? 1 : 0;
    check(lit == want && white == want, "no texel outside the glyph cells is lit");
    const FontGlyphSrc* space = find(a, ' ');
    const FontGlyphSrc* zhe = find(a, 0x416);
    const FontGlyphSrc* last = find(a, 0x40);
    check(space != nullptr && space->x == 0 && space->y == 0 && space->w == 0 && space->advance == 6,
          "the lowest code point takes the first cell; a blank glyph has no width and a mono advance");
    check(zhe != nullptr && zhe->x == 2 * 7 && zhe->y == LINE && zhe->w == 7 && zhe->advance == 8,
          "a glyph wider than ASCII advances by its width plus one, in the second row");
    check(last != nullptr && last->x == 0 && last->y == LINE && last->advance == 6,
          "the 33rd glyph wraps to the second row of cells");
}

void refused(const std::string& json, const char* expected, const char* what) {
    FontAtlas a;
    std::string error;
    const bool ok = import(json, a, error);
    const bool named = error.find(expected) != std::string::npos;
    if (ok || !named) std::printf("    got: %s\n", ok ? "imported" : error.c_str());
    check(!ok && named, what);
}

void test_refusals() {
    const std::string q = "\"?\": [1, 2]";
    refused("{}", "mono.json:1:1: a font has 1 to 4096 glyphs", "an empty object");
    refused("[1]", "mono.json:1:1: expected a JSON object", "a top-level array");
    refused("{" + q + ", \"AB\": [1, 2]}", "mono.json:1:15: a glyph key is exactly one character", "two letters");
    refused("{" + q + ", \"\": [1, 2]}", "a glyph key is exactly one character", "an empty key");
    refused("{" + q + ", \"\x7F\": [1, 2]}", "a glyph of a control character", "DEL");
    refused("{" + q + ", \"\xC2\x80\": [1, 2]}", "a glyph of a control character", "a C1 control");
    refused("{" + q + ", \"A\": 3}", "a glyph is an array of row masks", "a glyph of one number");
    refused("{\"?\": []}", "a glyph has 1 to 64 rows", "no row");
    std::string rows65 = "0";
    for (int i = 1; i < 65; ++i) rows65 += ",0";
    refused("{\"?\": [" + rows65 + "]}", "a glyph has 1 to 64 rows", "65 rows");
    refused("{" + q + ", \"A\": [1, 2.5]}", "a row mask is an integer from 0 to 65535", "a fraction");
    refused("{" + q + ", \"A\": [1, -1]}", "a row mask is an integer from 0 to 65535", "a negative mask");
    refused("{" + q + ", \"A\": [1, 65536]}", "a row mask is an integer from 0 to 65535", "a mask past 16 bits");
    refused("{" + q + ", \"A\": [1, 2, 3]}", "every glyph has as many rows as the first one", "uneven rows");
    refused("{\"A\": [1, 2]}", "mono.json: font 'mono' has no '?' glyph", "no fallback glyph");
    FontAtlas a;
    std::string error;
    check(import("{\"?\": [65535]}", a, error) && a.font.glyphs[0].w == 16 && a.font.page_w == 16,
          "a 16-bit mask is a glyph 16 pixels wide");
}

} // namespace

int main() {
    std::printf("bitmask font import: JSON masks to an RGBA atlas and a glyph table\n");
    test_oracle();
    test_refusals();
    std::printf("framework-graphics-font-import: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
