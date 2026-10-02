#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "baker_guid.hpp"
#include "bakers.hpp"
#include "format.hpp"
#include "manifest_bake.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "png_fixture.hpp"

// Путь pixel и бейк по манифесту (спека #24, В4). Байты RGBA сверяются с оракулом, посчитанным из
// самой фикстуры, а не с выводом декодера; голден `bundle_hash` держит раскладку записи бандла.

using namespace asset;
using png_fixture::Image;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        ++failures;
    }
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

std::vector<uint8_t> decode(const Image& im, std::string& why) {
    std::vector<AssetInput> out;
    if (!bakers::pixel(png_fixture::encode(im), "t", out, why) || out.size() != 1) return {};
    return out[0].payload;
}

void same(const Image& im, const std::vector<uint8_t>& want, const char* what) {
    std::string why;
    check(decode(im, why) == want, std::string(what) + " decodes to the oracle RGBA " + why);
}

void refused(const Image& im, const char* part, const char* what) {
    std::string why;
    std::vector<AssetInput> out;
    check(!bakers::pixel(png_fixture::encode(im), "t", out, why), std::string(what) + " refused");
    check(has(why, part) && out.empty(), std::string(what) + ": '" + part + "' in: " + why);
}

void conversions() {
    same({2, 1, 8, 6, {}, {}, {1, 2, 3, 4, 250, 251, 252, 253}}, {1, 2, 3, 4, 250, 251, 252, 253},
         "RGBA");
    same({2, 1, 8, 2, {}, {}, {10, 20, 30, 40, 50, 60}}, {10, 20, 30, 255, 40, 50, 60, 255}, "RGB");
    same({2, 1, 8, 0, {}, {}, {7, 200}}, {7, 7, 7, 255, 200, 200, 200, 255}, "grey");
    same({1, 1, 8, 4, {}, {}, {90, 33}}, {90, 90, 90, 33}, "grey+alpha");
    // Палитра 2 бита: четыре пикселя в одном байте, старшие биты — левый пиксель. tRNS короче
    // палитры — у индексов без записи альфа 255.
    const std::vector<uint8_t> pal{255, 0, 0, 0, 255, 0, 0, 0, 255, 9, 8, 7};
    same({4, 1, 2, 3, pal, {0, 128}, {0x1B}},
         {255, 0, 0, 0, 0, 255, 0, 128, 0, 0, 255, 255, 9, 8, 7, 255}, "palette 2-bit + tRNS");
}

void refusals() {
    std::string why;
    std::vector<AssetInput> out;
    check(!bakers::pixel({'G', 'I', 'F', '8', '9', 'a', 0, 0, 0}, "t", out, why) &&
              has(why, "not a PNG file"),
          "non-PNG refused: " + why);
    refused({4, 4, 16, 6, {}, {}, {}}, "16 bits per channel", "16-bit RGBA");
    refused({4, 4, 16, 0, {}, {}, {}}, "16 bits per channel", "16-bit grey");
    refused({4096, 4096, 8, 6, {}, {}, {}}, "PNG is 4096x4096", "4096x4096");
    refused({1, 1, 8, 6, {}, {}, {}}, "corrupt PNG", "header without pixel data");
    // Граница с обеих сторон: 2048 проходит, 2049 — нет, по каждой оси.
    std::vector<uint8_t> row(2048, 0x40);
    check(decode({2048, 1, 8, 0, {}, {}, row}, why).size() == 2048u * 4, "2048x1 accepted: " + why);
    check(decode({1, 2048, 8, 0, {}, {}, row}, why).size() == 2048u * 4, "1x2048 accepted: " + why);
    row.push_back(0x40);
    refused({2049, 1, 8, 0, {}, {}, row}, "PNG is 2049x1", "2049x1");
    refused({1, 2049, 8, 0, {}, {}, row}, "PNG is 1x2049", "1x2049");
}

void entry() {
    std::vector<AssetInput> out;
    std::string why;
    const Image im{3, 2, 8, 6, {}, {}, std::vector<uint8_t>(24, 0x11)};
    check(bakers::pixel(png_fixture::encode(im), "street_tiles", out, why) && out.size() == 1,
          "pixel bakes one asset: " + why);
    if (out.size() != 1) return;
    const AssetInput& a = out[0];
    check(a.guid == bakers::guid_of("street_tiles"), "guid is fnv of the record name");
    check(a.type == AssetType::Texture && a.codec == Codec::Raw && a.residency == Residency::Stream,
          "Texture / Raw / Stream");
    check(a.tex_w == 3 && a.tex_h == 2 && a.tex_format == bakers::TEX_FORMAT_RGBA8_UNORM,
          "3x2 RGBA8Unorm");
    check(a.uncompressed_size == 24 && a.payload.size() == 24, "Raw payload is w*h*4 bytes");
}

void write(const std::string& path, const std::string& text) {
    codec::write_file(path, std::vector<uint8_t>(text.begin(), text.end()));
}

bool bake(const std::string& root, const std::string& text, manifest::Bake& b) {
    write(root + "/game.manifest", text);
    b = manifest::Bake{};
    b.manifest = root + "/game.manifest";
    b.tools = codec::Tools{"tint", root + "/no-such-basisu"};
    b.tmp = root + "/out.ktx2.tmp";
    return manifest::bake(b);
}

void manifests(const std::string& root) {
    platform::ensure_dir(root + "/assets");
    codec::write_file(root + "/assets/a.png",
                      png_fixture::encode({2, 2, 8, 2, {}, {}, std::vector<uint8_t>(12, 0x80)}));
    codec::write_file(root + "/assets/deep.png", png_fixture::encode({1, 1, 16, 2, {}, {}, {}}));
    manifest::Bake b;
    check(bake(root, "texture|tiles|pixel|assets/a.png\n", b), "manifest bakes: " + b.error);
    check(b.deps == std::vector<std::string>{root + "/game.manifest", root + "/assets/a.png"},
          "deps are the manifest and every file read, in read order");
    const std::vector<uint8_t> grey{0x80, 0x80, 0x80, 0xFF};
    std::vector<uint8_t> want;
    for (int i = 0; i < 4; ++i) want.insert(want.end(), grey.begin(), grey.end());
    check(b.assets.size() == 1 && b.assets[0].payload == want, "manifest payload matches the oracle");
    const std::vector<uint8_t> bundle = write_bundle(b.assets);
    uint64_t hash = 0;
    if (bundle.size() >= sizeof(BundleHeader))
        std::memcpy(&hash, bundle.data() + offsetof(BundleHeader, bundle_hash), sizeof(hash));
    std::printf("  bundle_hash = 0x%016llx\n", static_cast<unsigned long long>(hash));
    check(hash == 0x187cffecad3f8d70ull, "golden bundle_hash of a one-texture pixel manifest");

    check(!bake(root, "# x\ntexture|deep|pixel|assets/deep.png\n", b), "16-bit file fails the bake");
    check(has(b.error, "game.manifest:2: ") && has(b.error, "assets/deep.png: PNG has 16 bits"),
          "error names manifest line and file: " + b.error);
    check(!bake(root, "texture|deep|hd|assets/deep.png\n", b) && has(b.error, "16 bits"),
          "hd checks the PNG before basisu: " + b.error);
    check(!bake(root, "texture|tiles|hd|assets/a.png\n", b) && has(b.error, "hd bake failed"),
          "hd without basisu names the way out: " + b.error);
    check(!bake(root, "texture|tiles|pixel|assets/A.png\n", b) && has(b.error, ":1: ") &&
              has(b.error, "spelled 'a.png'"),
          "case mismatch fails with the line: " + b.error);
    check(!bake(root, "texture|t|pixel|a.png\ntexture|t|pixel|a.png\n", b) &&
              has(b.error, ":2: duplicate name"),
          "duplicate fails with the line: " + b.error);
    check(!bake(root + "/missing", "", b) && has(b.error, "cannot read the manifest"),
          "unreadable manifest refused: " + b.error);
    check(!bake(root, std::string((1u << 20) + 1, '#'), b) && has(b.error, "over 1 MiB"),
          "manifest over the cap refused: " + b.error);
    platform::remove_file(root + "/assets/a.png");
    platform::remove_file(root + "/assets/deep.png");
    platform::remove_file(root + "/game.manifest");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string root =
        platform::exe_dir() + "/assetc_bake_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("assetc-bake: cannot create %s\n", root.c_str());
        return 3;
    }
    conversions();
    refusals();
    entry();
    manifests(root);
    std::printf("assetc-bake: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
