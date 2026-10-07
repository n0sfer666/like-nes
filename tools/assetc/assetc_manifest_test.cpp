#include <cstdio>
#include <string>
#include <vector>

#include "assetc_depfile.hpp"
#include "assetc_manifest.hpp"
#include "codec.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"

// Грамматика манифеста, сверка путей с диском и depfile (спека #24, В4). Каждый отказ сверяется
// по номеру строки и по куску текста: отказ «не та строка» ведёт автора манифеста не туда.

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

void rejects(const char* text, uint32_t line, const char* part) {
    std::vector<manifest::Record> recs;
    manifest::Error err;
    const bool ok = manifest::parse(text, recs, err);
    check(!ok, std::string("rejected: ") + text);
    check(err.line == line, std::string("line ") + std::to_string(line) + " for: " + text +
                                " (got " + std::to_string(err.line) + ")");
    check(has(err.message, part), std::string("'") + part + "' in: " + err.message);
}

void grammar() {
    std::vector<manifest::Record> recs;
    manifest::Error err;
    const char* good = "# street\n"
                       "\n"
                       "texture | street_tiles | pixel | assets/warped-city/tileset.png  # tiles\r\n"
                       "texture|hero.v2|hd|hero.png\n"
                       "level|level1|tiled|content/level1.tmj\n"
                       "level | level2 | tiled | content/level2.tmj | viewport\n"
                       "font|mono|bitmask|fonts/mono.json\n"
                       "credits | credits | assets/credits.txt\n";
    check(manifest::parse(good, recs, err), "good manifest parses: " + err.message);
    check(recs.size() == 6, "six records");
    if (recs.size() == 6)
        check(recs[4].kind == "font" && recs[4].codec == "bitmask" && recs[4].path == "fonts/mono.json" &&
                  recs[5].kind == "credits" && recs[5].name == "credits" && recs[5].codec.empty() &&
                  recs[5].path == "assets/credits.txt",
              "font and credits record fields; credits has no codec");
    if (recs.size() == 6) {
        check(recs[0].line == 3 && recs[0].name == "street_tiles" && recs[0].codec == "pixel" &&
                  recs[0].path == "assets/warped-city/tileset.png",
              "first record fields, comment and CR stripped");
        check(recs[1].line == 4 && recs[1].name == "hero.v2" && recs[1].codec == "hd",
              "second record fields");
        check(recs[0].kind == "texture" && recs[2].kind == "level" && recs[2].name == "level1" &&
                  recs[2].codec == "tiled" && recs[2].path == "content/level1.tmj",
              "level record fields");
        check(!recs[2].viewport && recs[3].viewport && recs[3].path == "content/level2.tmj",
              "viewport option only on the record that names it");
    }

    rejects("texture|a|pixel|a.png\ntexture|b|pixel|b.png\ntexture|a|hd|c.png\n", 3,
            "duplicate name 'a' (first on line 1)");
    rejects("texture|a|a.png\n", 1, "needs a codec");
    rejects("texture|a|pixel\n", 1, "needs a path: texture|<name>|pixel|<path>");
    rejects("\xEF\xBB\xBFtexture|a|pixel|a.png\n", 1, "UTF-8 BOM");
    rejects("texture|a|pixel|a.png|x\n", 1, "has 5 fields");
    rejects("texture|a|lossy|a.png\n", 1, "unknown codec 'lossy'");
    rejects("texture|a b|pixel|a.png\n", 1, "name 'a b'");
    rejects("texture||pixel|a.png\n", 1, "name ''");
    rejects("\nlevel|l1|levels/one.tmj\n", 2, "level record expected as level|<name>|tiled|<path>.tmj");
    rejects("level|l1|tmx|levels/one.tmj\n", 1, "level record expected as");
    rejects("level|l1|tiled|levels/one.tmx\n", 1, "must be a Tiled JSON map (.tmj)");
    rejects("level|l1|tiled|levels/one.tmj|fit\n", 1, "unknown level option 'fit', expected viewport");
    rejects("level|l1|tiled|levels/one.tmj|viewport|x\n", 1,
            "level record expected as level|<name>|tiled|<path>.tmj or level|<name>|tiled|<path>.tmj|viewport");
    rejects("level|l1|tiled|levels/one.tmx|viewport\n", 1, "must be a Tiled JSON map (.tmj)");
    rejects("level|l1|tiled|../one.tmj\n", 1, "uses '..'");
    rejects("level|l 1|tiled|one.tmj\n", 1, "name 'l 1'");
    rejects("texture|a|pixel|a.png\nlevel|a|tiled|a.tmj\n", 2, "duplicate name 'a'");
    rejects("texture|visual|pixel|a.png\n", 1, "reserved for the level, clip and font sections");
    rejects("texture|tilemap|pixel|a.png\n", 1, "reserved for the level, clip and font sections");
    rejects("texture|objects|pixel|a.png\n", 1, "reserved for the level, clip and font sections");
    rejects("clips|hero|hero.json\n", 1, "clips record expected as clips|<name>|aseprite|<path>.json");
    rejects("clips|hero|gif|hero.json\n", 1, "clips record expected as");
    rejects("clips|hero|aseprite|hero.sheet\n", 1, "clips path 'hero.sheet' must end in .json");
    rejects("clips|hero|sheet|hero.json\n", 1, "clips path 'hero.json' must end in .sheet");
    rejects("clips|hero|sheet|../hero.sheet\n", 1, "uses '..'");
    rejects("texture|clips|pixel|a.png\n", 1, "reserved for the level, clip and font sections");
    rejects("sound|a|a.wav\n", 1, "(this assetc bakes: texture, level, clips, font, credits, fighter)");
    rejects("font|mono|bitmask|mono.png\n", 1, "font path 'mono.png' must end in .json");
    rejects("font|mono|ttf|mono.json\n", 1, "font record expected as font|<name>|bitmask|<path>.json");
    rejects("font|mono|mono.json\n", 1, "font record expected as");
    rejects("font|fonts|bitmask|mono.json\n", 1, "font name 'fonts' is reserved for the level, clip and font sections");
    rejects("texture|fonts|pixel|a.png\n", 1, "texture name 'fonts' is reserved");
    rejects("credits|assets/credits.txt\n", 1, "credits record expected as credits|<name>|<path>.txt");
    rejects("credits|c|pixel|credits.txt\n", 1, "credits record expected as");
    rejects("credits|c|credits.toml\n", 1, "credits path 'credits.toml' must end in .txt");
    rejects("credits|clips|credits.txt\n", 1, "credits name 'clips' is reserved");
    rejects("credits|c|../credits.txt\n", 1, "uses '..'");
    rejects("texture|mono|pixel|a.png\nfont|mono|bitmask|m.json\n", 2, "duplicate name 'mono'");
    rejects("texture|a|pixel|../outside.png\n", 1, "uses '..'");
    rejects("texture|a|pixel|x/../a.png\n", 1, "uses '..'");
    rejects("texture|a|pixel|/etc/a.png\n", 1, "absolute");
    rejects("texture|a|pixel|C:/a.png\n", 1, "absolute");
    rejects("texture|a|pixel|C:a.png\n", 1, "absolute");
    rejects("texture|a|pixel|assets/a:b.png\n", 1, "uses ':'");
    rejects("texture|a|pixel|a.png:s\n", 1, "uses ':'");
    rejects("texture|a|pixel|dir\\a.png\n", 1, "uses '\\'");
    rejects("texture|a|pixel|dir//a.png\n", 1, "empty or '.' segment");
    rejects("texture|a|pixel|./a.png\n", 1, "empty or '.' segment");
    rejects("texture|a|pixel|\n", 1, "empty path");
    rejects("", 0, "no records");
    rejects("# only comments\n\n  \n", 0, "no records");
}

void paths(const std::string& root) {
    platform::ensure_dir(root + "/Tiles");
    codec::write_file(root + "/Tiles/sheet.png", {1});
    std::string full, why;
    check(manifest::resolve(root, "Tiles/sheet.png", full, why), "exact case resolves: " + why);
    check(full == root + "/Tiles/sheet.png", "resolved path joins base and record: " + full);

    check(!manifest::resolve(root, "tiles/sheet.png", full, why), "directory case mismatch rejected");
    check(has(why, "'tiles' is spelled 'Tiles' on disk"), "case message names both: " + why);
    check(!manifest::resolve(root, "Tiles/Sheet.png", full, why), "file case mismatch rejected");
    check(has(why, "'Sheet.png' is spelled 'sheet.png'"), "file case message: " + why);
    check(!manifest::resolve(root, "Tiles/none.png", full, why), "missing file rejected");
    check(has(why, "not found: no 'none.png'"), "missing message: " + why);
    check(!manifest::resolve(root, "Tiles", full, why), "directory as file rejected");
    check(has(why, "is a directory"), "directory message: " + why);
    platform::remove_file(root + "/Tiles/sheet.png");
}

void depfiles() {
    const std::vector<std::string> deps{"m.manifest", "my dir/a#1.png", "c:\\x\\$y.png"};
    const std::string win = depfile::text("out/game.bundle", deps, true);
    check(win == "out/game.bundle: m.manifest my\\ dir/a\\#1.png c:/x/$$y.png\n",
          "depfile escapes space, '#', '$' and turns a separator '\\' into '/': " + win);
    const std::string posix = depfile::text("out/game.bundle", deps, false);
    check(posix == "out/game.bundle: m.manifest my\\ dir/a\\#1.png c:\\x\\$$y.png\n",
          "a '\\' that is part of a POSIX name stays: " + posix);
    check(depfile::text("b", {}, false) == "b:\n", "no deps is still one well-formed line");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string root =
        platform::exe_dir() + "/assetc_manifest_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("assetc-manifest: cannot create %s\n", root.c_str());
        return 3;
    }
    grammar();
    paths(root);
    depfiles();
    std::printf("assetc-manifest: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
