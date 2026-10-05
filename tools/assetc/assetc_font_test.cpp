#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "baker_guid.hpp"
#include "credits_read.hpp"
#include "font_read.hpp"
#include "format.hpp"
#include "manifest_bake.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"

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

const char* const MANIFEST = "font|mono|bitmask|art/mono.json\n"
                             "credits|credits|art/credits.txt\n";

const char* const FONT = "{\"A\": [7, 5, 7], \"?\": [3, 2, 2], \" \": [0, 0, 0]}";

const char* const CREDITS = "# generated\n"
                            "credit | Warped City | Ansimuz | CC0-1.0 | https://ansimuz.itch.io/warped-city\n"
                            "credit | monogram | datagoblin | CC0-1.0 | https://datagoblin.itch.io/monogram\n"
                            "attribution | Font by datagoblin\n";

void write(const std::string& path, const std::string& text) {
    codec::write_file(path, std::vector<uint8_t>(text.begin(), text.end()));
}

bool bake(const std::string& root, const std::string& font, const std::string& credits, manifest::Bake& b) {
    platform::ensure_dir(root + "/art");
    write(root + "/art/mono.json", font);
    write(root + "/art/credits.txt", credits);
    write(root + "/game.manifest", MANIFEST);
    b = manifest::Bake{};
    b.manifest = root + "/game.manifest";
    b.tools = codec::Tools{"tint", root + "/no-such-basisu"};
    b.tmp = root + "/out.ktx2.tmp";
    return manifest::bake(b);
}

const AssetInput* find(const manifest::Bake& b, const char* name, AssetType type) {
    for (const AssetInput& a : b.assets)
        if (a.guid == bakers::guid_of(name) && a.type == type) return &a;
    return nullptr;
}

void fonts(const manifest::Bake& b) {
    const AssetInput* tex = find(b, "mono", AssetType::Texture);
    const AssetInput* table = find(b, "fonts", AssetType::Raw);
    check(tex != nullptr && table != nullptr, "font texture and fonts section by guid");
    if (tex == nullptr || table == nullptr) return;
    framework::graphics::FontTable t;
    check(t.open(table->payload.data(), table->payload.size()), "fonts section opens");
    const framework::graphics::FontView f = t.find("mono");
    check(f.row != nullptr && f.glyphs.size() == 3, "font 'mono' with three glyphs");
    if (f.row == nullptr) return;
    check(f.row->texture_guid == tex->guid && f.row->page_w == tex->tex_w && f.row->page_h == tex->tex_h,
          "the font row points at its atlas texture");
    check(tex->tex_w == 9 && tex->tex_h == 3 && tex->payload.size() == 9u * 3u * 4u, "atlas: three cells 3x3");
    const framework::graphics::FontGlyph* a = framework::graphics::font_glyph(f, 'A');
    check(a != nullptr && a->w == 3 && a->advance == 4, "glyph 'A' keeps its width and the mono advance");
}

void credits(const manifest::Bake& b) {
    const AssetInput* table = find(b, "credits", AssetType::Raw);
    check(table != nullptr, "credits section named by the record");
    if (table == nullptr) return;
    framework::core::CreditTable t;
    check(t.open(table->payload.data(), table->payload.size()) && t.count() == 2, "credits section opens with 2");
    if (t.count() != 2) return;
    const framework::core::Credit c = t.at(1);
    check(std::strcmp(c.pack, "monogram") == 0 && std::strcmp(c.attribution, "Font by datagoblin") == 0 &&
              std::strcmp(t.at(0).attribution, "") == 0,
          "credit fields and the optional attribution survive the bake");
}

void baked(const std::string& root) {
    manifest::Bake b;
    check(bake(root, FONT, CREDITS, b), "font and credits manifest bakes: " + b.error);
    const std::vector<std::string> deps{root + "/game.manifest", root + "/art/mono.json", root + "/art/credits.txt"};
    check(b.deps == deps, "deps: manifest, the font JSON, then the credits text");
    check(b.assets.size() == 3, "atlas texture, credits section and fonts section");
    fonts(b);
    credits(b);
    const std::vector<uint8_t> bundle = write_bundle(b.assets);
    uint64_t hash = 0;
    if (bundle.size() >= sizeof(BundleHeader))
        std::memcpy(&hash, bundle.data() + offsetof(BundleHeader, bundle_hash), sizeof(hash));
    std::printf("  bundle_hash = 0x%016llx\n", static_cast<unsigned long long>(hash));
    check(hash == 0xcdcd2840d0b4c917ull, "golden bundle_hash of the font and credits manifest");
}

void refused(const std::string& root, const std::string& font, const std::string& credits, const char* part,
             const char* what) {
    manifest::Bake b;
    check(!bake(root, font, credits, b) && has(b.error, part), std::string(what) + ": '" + part + "' in: " + b.error);
}

void refusals(const std::string& root) {
    refused(root, "{\"A\": [7, 5, 7]}", CREDITS, "game.manifest:1: ", "font refusal names the record line");
    refused(root, "{\"A\": [7, 5, 7]}", CREDITS, "has no '?' glyph", "a font without '?' is refused");
    refused(root, "{\"A\": [7, 5], \"?\": [3, 2, 2]}", CREDITS, "/art/mono.json:", "JSON refusal names the font file");
    refused(root, FONT, "# only a comment\n", "game.manifest:2: ", "credits refusal names the record line");
    refused(root, FONT, "# only a comment\n", "/art/credits.txt: no credit line", "credits without a credit line");
    refused(root, FONT, "credit | a | b | c\n", "/art/credits.txt:1: ", "credits refusal names its own line");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string root = platform::exe_dir() + "/assetc_font_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("assetc-font: cannot create %s\n", root.c_str());
        return 3;
    }
    baked(root);
    refusals(root);
    for (const char* f : {"/art/mono.json", "/art/credits.txt", "/game.manifest"})
        platform::remove_file(root + f);
    std::printf("assetc-font: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
