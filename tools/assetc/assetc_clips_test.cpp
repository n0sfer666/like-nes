#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "baker_guid.hpp"
#include "clip_fixture.hpp"
#include "clip_read.hpp"
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

const char* const MANIFEST = "texture|brawler_sheet|pixel|art/brawler.png\n"
                             "clips|brawler|aseprite|art/brawler.json\n"
                             "clips|thug|sheet|art/thug.sheet\n";

void write(const std::string& path, const std::string& text) {
    codec::write_file(path, std::vector<uint8_t>(text.begin(), text.end()));
}

void tree(const std::string& root, const std::string& json, const std::string& sheet) {
    platform::ensure_dir(root + "/art");
    codec::write_file(root + "/art/brawler.png",
                      png_fixture::encode({128, 32, 8, 6, {}, {}, std::vector<uint8_t>(128 * 32 * 4, 0x40)}));
    write(root + "/art/brawler.json", json);
    write(root + "/art/thug.sheet", sheet);
}

bool bake(const std::string& root, const std::string& text, manifest::Bake& b) {
    write(root + "/game.manifest", text);
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
    tree(root, clip_fixture::BRAWLER_JSON, clip_fixture::BRAWLER_SHEET);
    manifest::Bake b;
    check(bake(root, MANIFEST, b), "clips manifest bakes: " + b.error);
    const std::vector<std::string> deps{root + "/game.manifest", root + "/art/brawler.png",
                                        root + "/art/brawler.json", root + "/art/thug.sheet"};
    check(b.deps == deps, "deps: manifest, the sheet PNG, then both clip sources");
    check(b.assets.size() == 2, "one texture and one clips section");
    const AssetInput* clips = find(b, "clips");
    check(clips != nullptr, "clips section named by guid");
    if (clips == nullptr) return;
    framework::graphics::ClipTable t;
    check(t.open(clips->payload.data(), clips->payload.size()), "clips section opens");
    check(t.count() == 10, "five clips from each record");
    const framework::graphics::ClipView punch = t.find("brawler/punch");
    const framework::graphics::ClipView thug = t.find("thug/punch");
    check(punch.row != nullptr && thug.row != nullptr, "clip names carry the record name");
    if (punch.row == nullptr || thug.row == nullptr) return;
    check(punch.row->texture_guid == bakers::guid_of("brawler_sheet") &&
              thug.row->texture_guid == punch.row->texture_guid,
          "both records point at the texture record by guid");
    check(std::strcmp(framework::graphics::clip_event_name(thug, thug.clip.frames[1].event), "swing") == 0,
          "sheet event survives the bake");
    const std::vector<uint8_t> bundle = write_bundle(b.assets);
    uint64_t hash = 0;
    if (bundle.size() >= sizeof(BundleHeader))
        std::memcpy(&hash, bundle.data() + offsetof(BundleHeader, bundle_hash), sizeof(hash));
    std::printf("  bundle_hash = 0x%016llx\n", static_cast<unsigned long long>(hash));
    check(hash == 0x75c9bd832588a973ull, "golden bundle_hash of the clips manifest");
}

void refused(const std::string& root, const std::string& json, const std::string& sheet, const std::string& text,
             const char* part, const char* what) {
    tree(root, json, sheet);
    manifest::Bake b;
    check(!bake(root, text, b) && has(b.error, part), std::string(what) + ": '" + part + "' in: " + b.error);
}

std::string with(std::string text, const char* from, const char* to) {
    const size_t at = text.find(from);
    if (at != std::string::npos) text.replace(at, std::strlen(from), to);
    return text;
}

void refusals(const std::string& root) {
    const std::string json = clip_fixture::BRAWLER_JSON;
    const std::string sheet = clip_fixture::BRAWLER_SHEET;
    refused(root, json, sheet, "clips|brawler|aseprite|art/brawler.json\n",
            "game.manifest:1: ", "JSON refusal names the record line");
    refused(root, json, sheet, "clips|brawler|aseprite|art/brawler.json\n", "/art/brawler.json:15:10: ",
            "JSON refusal names the position of 'meta'");
    refused(root, json, sheet, "clips|brawler|aseprite|art/brawler.json\n",
            "image 'art/brawler.png' is not a texture record; add a line such as texture|brawler|pixel|art/brawler.png",
            "sheet PNG without a texture record");
    refused(root, json, with(sheet, "box|idle|1|", "box|idle|7|"), MANIFEST, "game.manifest:3: ",
            "sheet refusal names the record line");
    refused(root, json, with(sheet, "box|idle|1|", "box|idle|7|"), MANIFEST, "art/thug.sheet:6: ",
            "sheet refusal names the sheet line");
    refused(root, with(json, "\"name\": \"hit0\"", "\"name\": \"hit4\""), sheet, MANIFEST, "is not a box",
            "Aseprite refusal reaches the manifest");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string root = platform::exe_dir() + "/assetc_clips_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("assetc-clips: cannot create %s\n", root.c_str());
        return 3;
    }
    baked(root);
    refusals(root);
    for (const char* f : {"/art/brawler.png", "/art/brawler.json", "/art/thug.sheet", "/game.manifest"})
        platform::remove_file(root + f);
    std::printf("assetc-clips: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
