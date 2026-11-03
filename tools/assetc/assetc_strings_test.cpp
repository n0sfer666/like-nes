#include <algorithm>
#include <cstdio>
#include <cstring>

#include "assetc_manifest.hpp"
#include "assetc_manifest_mode.hpp"
#include "baker_guid.hpp"
#include "manifest_bake.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "strings.hpp"

namespace {
using namespace asset;
int failures;
void check(bool ok, const std::string& why) { if (!ok) { ++failures; std::printf("assetc-strings: FAIL %s\n", why.c_str()); } }
void write(const std::string& path, const std::string& text) {
    check(codec::write_file(path, {text.begin(), text.end()}), "write fixture " + path);
}
const char* FONT = "{\"A\": [1], \"?\": [1], \" \": [0], \"\xD0\x96\": [1]}";
const char* MANIFEST = "strings|labels|tsv|labels.tsv|mono\nfont|mono|bitmask|mono.json\n";
const char* TSV = "key\ten\tru\nmenu.go\tA\t\ngrid.name\tA\t\xD0\x96\n";

void grammar() {
    std::vector<manifest::Record> records; manifest::Error error;
    check(manifest::parse("strings|labels|tsv|labels.tsv|viewport\n", records, error) && records.size() == 1 &&
          records[0].font == "viewport" && !records[0].viewport, "fifth field is font, not the level viewport flag");
    for (const auto* line : {"strings|s|tsv|a.tsv", "strings|s|json|a.tsv|mono", "strings|s|tsv|a.txt|mono",
         "strings|s|tsv|a.tsv|bad font", "strings|s|tsv|a.tsv|mono|extra", "strings|s|tsv|../a.tsv|mono",
         "strings|fonts|tsv|a.tsv|mono", "strings|s|tsv|a.tsv|mono\nfont|s|bitmask|mono.json"})
        check(!manifest::parse(line, records, error) && !error.message.empty(), std::string("bad grammar refused: ") + line);
}
bool bake(const std::string& root, const std::string& tsv, const std::string& manifest_text, bool strict,
          manifest::Bake& b) {
    write(root + "/labels.tsv", tsv); write(root + "/game.manifest", manifest_text);
    b = {}; b.manifest = root + "/game.manifest"; b.strict = strict;
    return manifest::bake(b);
}
void baked(const std::string& root) {
    write(root + "/mono.json", FONT);
    manifest::Bake b;
    check(bake(root, TSV, MANIFEST, false, b), "font declared after strings resolves: " + b.error);
    const std::vector<std::string> deps{root + "/game.manifest", root + "/labels.tsv", root + "/mono.json"};
    check(b.deps == deps && b.assets.size() == 3, "TSV and font in deps; texture/fonts/strings only");
    const auto found = std::find_if(b.assets.begin(), b.assets.end(), [](const auto& a) { return a.guid == bakers::guid_of("labels"); });
    framework::ui::Strings table;
    check(found != b.assets.end() && found->type == AssetType::Raw && found->codec == Codec::Raw &&
          found->residency == Residency::Mmap && table.open(found->payload.data(), found->payload.size()) &&
          table.count() == 2 && table.fallbacks() == 1 && table.find("menu.go", framework::ui::Language::Ru) == "A" &&
          table.find("grid.name", framework::ui::Language::Ru) == "\xD0\x96" && std::string_view(table.font_name()) == "mono",
          "named raw LNST retains translations, fallback and font");
    check(!bake(root, TSV, MANIFEST, true, b) && b.error.find("game.manifest:1:") != std::string::npos &&
          b.error.find("menu.go") != std::string::npos && b.error.find("--strict") != std::string::npos, "strict refuses with manifest line and key");
    check(bake(root, "key\ten\tru\nmenu.go\tA\tA\n", MANIFEST, true, b), "explicit identical translation passes strict");
    check(!bake(root, TSV, "strings|labels|tsv|labels.tsv|missing\nfont|mono|bitmask|mono.json\n", false, b) &&
          b.error.find("missing font 'missing'") != std::string::npos, "missing named font is not silently substituted");
    check(!bake(root, "key\ten\tru\ngrid.name\tA\t\xD0\xAF\n", MANIFEST, false, b) &&
          b.error.find("grid.name") != std::string::npos && b.error.find("U+042F") != std::string::npos, "alphabet coverage names key and code point");
    check(!bake(root, "key\ten\tru\ngrid.ip\tA\tA\tunexpected\n", MANIFEST, false, b) &&
          b.error.find("labels.tsv:2:") != std::string::npos, "TSV error names source line");
    check(!bake(root, "key\ten\tru\nx\tA\tA\nx\tA\tA\n", MANIFEST, false, b) &&
          b.error.find("duplicate") != std::string::npos, "duplicate key refused through manifest");
    check(!bake(root, "", MANIFEST, false, b) && b.error.find("no string keys") != std::string::npos, "empty source safely refused");
    check(bake(root, "key\ten\tru\nlong\t" + std::string((1u << 20) + 32, 'A') + "\tA\n", MANIFEST, true, b),
          "strings use their 16 MiB cap, not the generic 1 MiB text-record cap");
    check(!bake(root, std::string(framework::ui::MAX_STRINGS_BYTES + 1, 'A'), MANIFEST, false, b) &&
          b.error.find("over 16 MiB") != std::string::npos, "oversized source refused by capped file reader");
}
void cli(const std::string& root) {
    write(root + "/game.manifest", MANIFEST); write(root + "/labels.tsv", TSV);
    const std::string out = root + "/out.bundle", dep = root + "/out.d";
    const std::vector<std::string> prefix{platform::exe_path(), "--manifest", root + "/game.manifest", out};
    auto invoke = [&](std::vector<std::string> tail, int code, const char* expected) {
        auto args = prefix; args.insert(args.end(), tail.begin(), tail.end());
        std::string log; platform::ExitStatus status;
        check(platform::run_capture(args, log, status) && status.kind == platform::ExitKind::Exited && status.code == code &&
              log.find(expected) != std::string::npos, std::string("CLI exit/diagnostic: ") + expected + "\n" + log);
    };
    invoke({"--depfile", dep}, 0, "strings labels: keys=2 fallbacks=1");
    std::string deps;
    check(platform::read_text(dep, deps) && deps.find("labels.tsv") != std::string::npos && deps.find("mono.json") != std::string::npos,
          "CLI emits actual TSV/font dependency file");
    std::vector<uint8_t> before, after;
    check(platform::read_bytes(out, before) && !before.empty(), "CLI produced a bundle");
    invoke({"--strict"}, 1, "--strict");
    check(platform::read_bytes(out, after) && after == before, "strict refusal preserves existing output");
    invoke({"--strict", "--depfile"}, 2, "unexpected argument '--depfile'");
    invoke({"--depfile", "--strict"}, 2, "unexpected argument '--depfile'");
    invoke({"--basisu", "--strict"}, 2, "unexpected argument '--basisu'");
    invoke({"--strict=yes"}, 2, "unexpected argument '--strict=yes'");
    write(root + "/labels.tsv", "key\ten\tru\nmenu.go\tA\tA\n");
    invoke({"--depfile", dep, "--strict"}, 0, "strings labels: keys=1 fallbacks=0");
    check(platform::read_bytes(out, after) && after != before, "strict complete translation publishes changed bundle");
    platform::remove_file(out); platform::remove_file(dep);
}
}
int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "--manifest") == 0) return asset::manifest::run_manifest(argc, argv, {});
    const auto root = platform::exe_dir() + "/assetc_strings_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) return 3;
    grammar(); baked(root); cli(root);
    for (const auto* file : {"/game.manifest", "/mono.json", "/labels.tsv"}) platform::remove_file(root + file);
    std::puts(failures ? "assetc-strings: FAIL" : "assetc-strings: PASS grammar, coverage, forward fonts, CLI strict/summary/depfile");
    return failures ? 1 : 0;
}
