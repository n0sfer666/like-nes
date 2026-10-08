#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "baker_guid.hpp"
#include "clip_fixture.hpp"
#include "fighter_read.hpp"
#include "format.hpp"
#include "manifest_bake.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "png_fixture.hpp"

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

const char* const MANIFEST = "fighter|brawler.fighter|art/brawler.fighter\n"
                             "texture|brawler_sheet|pixel|art/brawler.png\n"
                             "clips|brawler|aseprite|art/brawler.json\n";

const char* const FIGHTER = "sheet   | brawler\n"
                            "speed_x | 2\n"
                            "speed_z | 1\n"
                            "run_x   | 3\n"
                            "gravity | 0.5\n"
                            "jump_vy | 6\n"
                            "depth   | 4\n"
                            "hp      | 100\n"
                            "down    | 30\n"
                            "getup   | 24\n"
                            "move | punch | hit0\n"
                            "type      | light\n"
                            "damage    | 10\n"
                            "depth     | 5\n"
                            "hitstop   | 4\n"
                            "hitstun   | 12\n"
                            "knock_x   | 1\n"
                            "knock_y   | 0\n"
                            "hits_down | no\n";

void write(const std::string& path, const std::string& text) {
    codec::write_file(path, std::vector<uint8_t>(text.begin(), text.end()));
}

bool bake(const std::string& root, const std::string& manifest, const std::string& fighter, manifest::Bake& b) {
    platform::ensure_dir(root + "/art");
    codec::write_file(root + "/art/brawler.png",
                      png_fixture::encode({128, 32, 8, 6, {}, {}, std::vector<uint8_t>(128 * 32 * 4, 0x40)}));
    write(root + "/art/brawler.json", clip_fixture::BRAWLER_JSON);
    write(root + "/art/brawler.fighter", fighter);
    write(root + "/game.manifest", manifest);
    b = manifest::Bake{};
    b.manifest = root + "/game.manifest";
    b.tools = codec::Tools{"tint", root + "/no-such-basisu"};
    b.tmp = root + "/out.ktx2.tmp";
    return manifest::bake(b);
}

const AssetInput* find(const manifest::Bake& b, const char* name) {
    for (const AssetInput& a : b.assets)
        if (a.guid == bakers::guid_of(name)) return &a;
    return nullptr;
}

void baked(const std::string& root) {
    manifest::Bake b;
    check(bake(root, MANIFEST, FIGHTER, b), "a fighter listed before its clips bakes: " + b.error);
    const std::vector<std::string> deps{root + "/game.manifest", root + "/art/brawler.png",
                                        root + "/art/brawler.fighter", root + "/art/brawler.json"};
    check(b.deps == deps, "deps: manifest, the sheet PNG, then the records in manifest order");
    check(b.assets.size() == 3, "one texture, one clips section, one fighter section");
    const AssetInput* fighter = find(b, "brawler.fighter");
    check(fighter != nullptr, "fighter section named by its record");
    if (fighter == nullptr) return;
    framework::brawl::FighterTable t;
    check(t.open(fighter->payload.data(), fighter->payload.size()), "fighter section opens");
    check(std::strcmp(t.name(), "brawler.fighter") == 0 && std::strcmp(t.sheet(), "brawler") == 0, "name and sheet");
    check(t.move_count() == 1 && std::strcmp(t.move_clip(0), "brawler/punch") == 0, "the move names its clip");
    const std::vector<uint8_t> bundle = write_bundle(b.assets);
    uint64_t hash = 0;
    if (bundle.size() >= sizeof(BundleHeader))
        std::memcpy(&hash, bundle.data() + offsetof(BundleHeader, bundle_hash), sizeof(hash));
    std::printf("  bundle_hash = 0x%016llx\n", static_cast<unsigned long long>(hash));
    check(hash == 0x321ba03d7b226405ull, "golden bundle_hash of the fighter manifest");
}

std::string with(std::string text, const char* from, const char* to) {
    const size_t at = text.find(from);
    if (at != std::string::npos) text.replace(at, std::strlen(from), to);
    return text;
}

void refused(const std::string& root, const std::string& manifest, const std::string& fighter, const char* part,
             const char* what) {
    manifest::Bake b;
    check(!bake(root, manifest, fighter, b) && has(b.error, part),
          std::string(what) + ": '" + part + "' in: " + b.error);
}

void refusals(const std::string& root) {
    const std::string kick = with(FIGHTER, "move | punch", "move | kick");
    refused(root, MANIFEST, kick, "game.manifest:1: ", "fighter refusal names the record line");
    refused(root, MANIFEST, kick, "/art/brawler.fighter:11: move 'kick' hit0: no clip 'brawler/kick' in the clips",
            "a move without its clip names the fighter line");
    refused(root, MANIFEST, with(FIGHTER, "punch | hit0", "punch | hit1"),
            "move 'punch' hit1: no frame of clip 'brawler/punch' carries this hit box", "a move without its box");
    refused(root, MANIFEST, "", "/art/brawler.fighter: the fighter file is empty", "a refusal without a line");
    refused(root, MANIFEST, with(FIGHTER, "hp      | 100", "hp      | lots"), "/art/brawler.fighter:8: hp must be",
            "a parse refusal names the fighter line");
    refused(root, with(MANIFEST, "art/brawler.fighter\n", "art/brawler.txt\n"), FIGHTER,
            "fighter path 'art/brawler.txt' must end in .fighter", "fighter path extension");
    refused(root, with(MANIFEST, "fighter|brawler.fighter|", "fighter|brawler.fighter|x|"), FIGHTER,
            "fighter record expected as fighter|<name>|<path>.fighter", "fighter record shape");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string root = platform::exe_dir() + "/assetc_fighter_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("assetc-fighter: cannot create %s\n", root.c_str());
        return 3;
    }
    baked(root);
    refusals(root);
    for (const char* f : {"/art/brawler.png", "/art/brawler.json", "/art/brawler.fighter", "/game.manifest"})
        platform::remove_file(root + f);
    std::printf("assetc-fighter: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
