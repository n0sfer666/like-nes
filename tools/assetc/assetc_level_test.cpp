#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "baker_guid.hpp"
#include "format.hpp"
#include "manifest_bake.hpp"
#include "map_bake.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "png_fixture.hpp"
#include "tiled_fixture.hpp"
#include "visual_read.hpp"

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

const char* const MANIFEST = "texture|city|pixel|art/city.png\n"
                             "level|one|tiled|levels/one.tmj\n"
                             "texture|deco|pixel|levels/deco.png\n"
                             "texture|sky|pixel|art/sky.png\n";

void write(const std::string& path, const std::string& text) {
    codec::write_file(path, std::vector<uint8_t>(text.begin(), text.end()));
}

void png(const std::string& path, uint32_t w, uint32_t h) {
    codec::write_file(path, png_fixture::encode({w, h, 8, 6, {}, {}, std::vector<uint8_t>(w * h * 4, 0x40)}));
}

void tree(const std::string& root, const std::string& tmj) {
    platform::ensure_dir(root + "/art");
    platform::ensure_dir(root + "/levels");
    png(root + "/art/city.png", 64, 32);
    png(root + "/art/sky.png", 128, 64);
    png(root + "/levels/deco.png", 32, 16);
    write(root + "/art/tiles.tsj", tiled_fixture::TSJ);
    write(root + "/levels/one.tmj", tmj);
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
    tree(root, tiled_fixture::TMJ);
    manifest::Bake b;
    check(bake(root, MANIFEST, b), "level manifest bakes: " + b.error);
    const std::vector<std::string> deps{root + "/game.manifest", root + "/art/city.png",
                                        root + "/levels/deco.png", root + "/art/sky.png",
                                        root + "/levels/one.tmj", root + "/art/tiles.tsj"};
    check(b.deps == deps, "deps: manifest, textures, then the map and its tileset");
    check(b.assets.size() == 6, "three textures and three level sections");
    const AssetInput* tilemap = find(b, "tilemap");
    const AssetInput* visual = find(b, "visual");
    check(tilemap != nullptr && visual != nullptr && find(b, "objects") != nullptr, "sections named by guid");
    if (tilemap == nullptr || visual == nullptr) return;
    std::vector<uint8_t> pipe;
    framework::tilemap::MapBakeError err;
    framework::tilemap::bake_maps(tiled_fixture::PIPE_MAP, pipe, err);
    check(tilemap->payload == pipe, "tilemap section equals the pipe-map LNTM byte for byte");
    framework::tilemap::VisualTable t;
    check(t.open(visual->payload.data(), visual->payload.size()), "visual section opens");
    const framework::tilemap::VisualMap m = t.find("one");
    check(m.tilesets.size() == 2 && m.tilesets[0].texture_guid == bakers::guid_of("city") &&
              m.tilesets[1].texture_guid == bakers::guid_of("deco"),
          "tilesets point at the texture records by guid");
    check(m.layers.size() == 3 && m.layers[0].image_guid == bakers::guid_of("sky"), "image layer points at sky");
    const std::vector<uint8_t> bundle = write_bundle(b.assets);
    uint64_t hash = 0;
    if (bundle.size() >= sizeof(BundleHeader))
        std::memcpy(&hash, bundle.data() + offsetof(BundleHeader, bundle_hash), sizeof(hash));
    std::printf("  bundle_hash = 0x%016llx\n", static_cast<unsigned long long>(hash));
    check(hash == 0x740cdb85a1f777eaull, "golden bundle_hash of the level manifest");
}

void refused(const std::string& root, const std::string& tmj, const std::string& text, const char* part,
             const char* what) {
    tree(root, tmj);
    manifest::Bake b;
    check(!bake(root, text, b) && has(b.error, part), std::string(what) + ": '" + part + "' in: " + b.error);
}

std::string with(std::string text, const char* from, const char* to) {
    const size_t at = text.find(from);
    if (at != std::string::npos) text.replace(at, std::strlen(from), to);
    return text;
}

void refusals(const std::string& root) {
    const std::string tmj = tiled_fixture::TMJ;
    refused(root, tmj, "level|one|tiled|levels/one.tmj\ntexture|city|pixel|art/city.png\n",
            "game.manifest:1: ", "level error names the level line");
    refused(root, tmj, "texture|city|pixel|art/city.png\nlevel|one|tiled|levels/one.tmj\n",
            "image 'levels/deco.png' is not a texture record; add a line such as texture|deco|pixel|levels/deco.png",
            "image without a texture record");
    refused(root, with(tmj, "../art/tiles.tsj", "../art/Tiles.tsj"), MANIFEST, "spelled 'tiles.tsj'",
            "tileset path case checked on disk");
    refused(root, with(tmj, "../art/tiles.tsj", "../../tiles.tsj"), MANIFEST, "leaves the manifest directory",
            "tileset outside the manifest directory");
    refused(root, with(tmj, "../art/tiles.tsj", "../art//tiles.tsj"), MANIFEST, "has an empty or '.' segment",
            "tileset path with an empty segment");
    refused(root, with(tmj, "../art/sky.png", "C:/art/sky.png"), MANIFEST, "must be relative and use '/'",
            "absolute image path");
    refused(root, with(tmj, R"("imagewidth":32,)", R"("imagewidth":48,)"), MANIFEST, "reload the image in Tiled",
            "PNG size checked against the tileset");
    refused(root, with(tmj, R"("infinite":false)", R"("infinite":true)"), MANIFEST, "levels/one.tmj:2:",
            "Tiled refusal carries the map file, line and column");
}

void uncovered(const std::string& root) {
    tree(root, tiled_fixture::TMJ);
    manifest::Bake b;
    const std::string expect = root + "/game.manifest:2: " + root + "/levels/one.tmj: map 'one': layer 'sky' "
                               "does not cover the view: y short by 140 px top, 124 px bottom";
    check(!bake(root, with(MANIFEST, "levels/one.tmj\n", "levels/one.tmj|viewport\n"), b) && b.error == expect,
          "level under the viewport option is refused by layer cover: " + b.error);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string root = platform::exe_dir() + "/assetc_level_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("assetc-level: cannot create %s\n", root.c_str());
        return 3;
    }
    baked(root);
    refusals(root);
    uncovered(root);
    for (const char* f : {"/art/city.png", "/art/sky.png", "/levels/deco.png", "/art/tiles.tsj", "/levels/one.tmj",
                          "/game.manifest"})
        platform::remove_file(root + f);
    std::printf("assetc-level: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
