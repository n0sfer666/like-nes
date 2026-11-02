#include <cstdio>
#include <string>
#include <vector>

#include "assetc_manifest.hpp"
#include "bundle_writer.hpp"
#include "codec.hpp"
#include "hash.hpp"
#include "manifest_bake.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_path.hpp"
#include "platform_process.hpp"
#include "png_fixture.hpp"
#include "viewport_settings.hpp"

namespace {
namespace mf = asset::manifest;
namespace gr = framework::graphics;
int fails = 0;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("  FAIL: %s\n", why); } }
const char* const RECORD = "texture|sky|pixel|sky.png\n";

void write(const std::string& path, const std::string& text) {
    check(asset::codec::write_file(path, std::vector<uint8_t>(text.begin(), text.end())), "fixture text writes");
}
void image(const std::string& root, uint32_t height) {
    check(asset::codec::write_file(root + "/sky.png", png_fixture::encode({1, height, 8, 6, {}, {},
        std::vector<uint8_t>(height * 4, 0x40)})), "fixture image writes");
}

bool bake(const std::string& root, const std::string& text, mf::Bake& out) {
    write(root + "/game.manifest", text);
    out = {};
    out.manifest = root + "/game.manifest";
    const bool ok = mf::bake(out);
    if (!ok) std::printf("  bake refusal: %s\n", out.error.c_str());
    return ok;
}

const asset::AssetInput* settings(const mf::Bake& b) {
    for (const auto& a : b.assets) if (a.guid == asset::fnv1a_str("viewport")) return &a;
    return nullptr;
}

void rejects(const std::string& text, uint32_t line, const char* why) {
    std::vector<mf::Record> records;
    mf::Error error;
    gr::ViewportSettings out{{1, 1}, {1, 1}};
    check(!mf::parse(text + RECORD, records, error, {}, &out) && error.line == line &&
          error.message.find(why) != std::string::npos && gr::viewport_settings_default(out),
          "invalid settings refuse at their line and reset pending settings");
}

std::string map(uint32_t height) {
    std::string cells = "0";
    for (uint32_t i = 1; i < 6000; ++i) cells += ",0";
    return R"({"type":"map","orientation":"orthogonal","renderorder":"right-down","infinite":false,
"width":100,"height":60,"tilewidth":16,"tileheight":16,"tilesets":[],"layers":[
{"type":"imagelayer","name":"sky","image":"sky.png","repeatx":true,"parallaxx":0,"parallaxy":0,"offsety":)" +
        std::to_string(-static_cast<int32_t>(height) / 2) +
        R"(},{"type":"tilelayer","name":"solid","width":100,"height":60,
"properties":[{"name":"collision","type":"bool","value":true}],"data":[)" + cells + "]}]}";
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    const std::string root = platform::exe_dir() + "/assetc_viewport_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) return 3;
    image(root, 312);
    mf::Bake plain, explicit_defaults, custom;
    check(bake(root, RECORD, plain) && settings(plain) == nullptr, "absent settings do not add a section");
    check(bake(root, std::string("zone|384|216\nzone_limit|576|312\n") + RECORD, explicit_defaults) &&
          asset::write_bundle(plain.assets) == asset::write_bundle(explicit_defaults.assets),
          "explicit defaults preserve every bundle byte");
    check(bake(root, std::string(RECORD) + "zone_limit|960|540\nzone|640|360\n", custom),
          "settings apply independently of their position in the manifest");
    gr::ViewportSettings read;
    const asset::AssetInput* asset = settings(custom);
    check(asset && gr::read_viewport_settings(asset->payload.data(), asset->payload.size(), read) &&
          read.zone.w == 640 && read.zone.h == 360 && read.limit.w == 960 && read.limit.h == 540,
          "nondefault dimensions reach the baked section");
    rejects("zone|640|360\nzone|640|360\n", 2, "duplicate zone");
    rejects("zone_limit|960|540\nzone_limit|960|540\n", 2, "duplicate zone_limit");
    for (const char* bad : {"0", "-1", "1.5", "32768", "4294967296", "no"})
        rejects(std::string("zone|640|") + bad + "\n", 1, "integers between");
    rejects("zone|640\n", 1, "expected as");
    rejects("zone|640|360|extra\n", 1, "expected as");
    rejects("texture|viewport|pixel|sky.png\n", 1, "reserved");
    write(root + "/level.tmj", map(312));
    const std::string level = std::string(RECORD) + "level|one|tiled|level.tmj|viewport\n";
    check(bake(root, level, plain), "standard sky covers the default viewport");
    const std::string wider = "zone|640|360\nzone_limit|960|540\n" + level;
    check(!bake(root, wider, custom) && custom.error.find("layer 'sky' does not cover") != std::string::npos,
          "control: the same sky refuses under the manifest's larger viewport");
    image(root, 540);
    write(root + "/level.tmj", map(540));
    check(bake(root, wider, custom), "an enlarged sky covers the configured viewport");
    for (const char* path : {"/sky.png", "/level.tmj", "/game.manifest"}) platform::remove_file(root + path);
    std::printf("assetc-viewport: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
