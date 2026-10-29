#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include "assetc_manifest.hpp"

using namespace asset;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        ++failures;
    }
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

bool no_bake(const char*, const std::string&, std::span<const framework::graphics::ClipSrc>, std::vector<uint8_t>&,
             manifest::KindError&) { return false; }

// Игровой вид (спека #29, В2а): запись `<вид>|<имя>|<путь><ext>` без кодека; битая таблица видов —
// безымянный, без точки, затеняющий встроенный или названный дважды — отказ до первой строки.
void kinds() {
    const manifest::Kind fighter[] = {{"fighter", ".fighter", no_bake}};
    const manifest::Kind bad[] = {{nullptr, ".x", no_bake}, {"bo ss", ".b", no_bake}, {"boss", "boss", no_bake},
                                  {"boss", ".boss", nullptr}, {"texture", ".png", no_bake},
                                  {"boss", ".", no_bake},   {"boss", nullptr, no_bake}, {"boss", ".a|b", no_bake},
                                  {"fighter", ".fighter", no_bake}, {"fighter", ".fight", no_bake}};
    const char* part[] = {"has no name", "must be letters", "must be '.' and a suffix", "has no name",
                          "shadows a built-in kind", "must be '.' and a suffix", "has no name",
                          "must be '.' and a suffix", "is listed twice"};
    const size_t size[] = {1, 1, 1, 1, 1, 1, 1, 1, 2};
    std::vector<manifest::Record> r;
    manifest::Error e;
    check(manifest::parse("fighter|a|f/a.fighter\n", r, e, fighter) && r[0].path == "f/a.fighter" && r[0].codec.empty(),
          "game kind record: " + e.message);
    check(!manifest::parse("fighter|a|f/a.fight\n", r, e, fighter) && has(e.message, "must end in .fighter"),
          "game kind extension: " + e.message);
    check(!manifest::parse("boss|a|b.boss\n", r, e, fighter) && has(e.message, "touch, fighter)"),
          "game kinds listed after the built-in ones: " + e.message);
    check(!manifest::parse("texture|a|pixel|a.png\n", r, e, {bad + 4, 1}) && has(e.message, "kind table: "),
          "parse refuses a broken kind table: " + e.message);
    for (size_t i = 0, at = 0; i < std::size(part); at += size[i++]) {
        std::string why;
        check(!manifest::kinds_ok({bad + at, size[i]}, why) && has(why, "kind table: ") && has(why, part[i]),
              std::string(part[i]) + ": " + why);
    }
}

} // namespace

int main() {
    kinds();
    std::printf("assetc-kinds: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
