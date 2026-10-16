#include <cstdio>
#include <string>
#include <vector>

#include "baker_guid.hpp"
#include "format.hpp"
#include "manifest_bake.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "touch_layout.hpp"

using namespace asset;

namespace {

int failures = 0;
int cases = 0;

void check(bool ok, const std::string& what) {
    ++cases;
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        ++failures;
    }
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

const char* const MANIFEST = "# the shooter's screen controls\n"
                             "touch|pad|layout|ui/touch.txt\n";

const char* const LAYOUT = "stick | 48\n"
                           "button | a | bottom-right | 72 | 64 | 40\n";

void write(const std::string& path, const std::string& text) {
    codec::write_file(path, std::vector<uint8_t>(text.begin(), text.end()));
}

bool bake(const std::string& root, const std::string& layout, manifest::Bake& b) {
    platform::ensure_dir(root + "/ui");
    write(root + "/ui/touch.txt", layout);
    write(root + "/game.manifest", MANIFEST);
    b = manifest::Bake{};
    b.manifest = root + "/game.manifest";
    b.tools = codec::Tools{"tint", root + "/no-such-basisu"};
    b.tmp = root + "/out.ktx2.tmp";
    return manifest::bake(b);
}

void baked(const std::string& root) {
    manifest::Bake b;
    check(bake(root, LAYOUT, b), "touch manifest bakes: " + b.error);
    const std::vector<std::string> deps{root + "/game.manifest", root + "/ui/touch.txt"};
    check(b.deps == deps, "deps: manifest, then the layout text");
    check(b.assets.size() == 1, "one asset: the layout section");
    if (b.assets.size() != 1) return;
    const AssetInput& a = b.assets[0];
    check(a.guid == bakers::guid_of("pad") && a.type == AssetType::Raw && a.codec == Codec::Raw &&
              a.residency == Residency::Mmap,
          "the section is a raw mmap table named by the record");
    framework::touch::Layout l;
    check(framework::touch::read_layout(a.payload.data(), a.payload.size(), l), "the section reads back");
    check(l.stick_radius == fix32::from_int(48) && l.button_count == 1 &&
              l.buttons[0].anchor == framework::touch::Anchor::BottomRight,
          "stick radius and the button corner survive the bake");
}

void refused(const std::string& root, const std::string& layout, const char* part, const char* what) {
    manifest::Bake b;
    check(!bake(root, layout, b) && has(b.error, part), std::string(what) + ": '" + part + "' in: " + b.error);
}

void refusals(const std::string& root) {
    refused(root, "button | a | bottom-right | 1 | 1 | 1\n", "game.manifest:2: ", "refusal names the record line");
    refused(root, "button | a | bottom-right | 1 | 1 | 1\n", "/ui/touch.txt: no stick line", "a layout without a stick");
    refused(root, "stick | 40\nbutton | a | middle | 1 | 1 | 1\n", "/ui/touch.txt:2: anchor 'middle'",
            "a layout refusal names its own line");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string root = platform::exe_dir() + "/assetc_touch_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("assetc-touch: cannot create %s\n", root.c_str());
        return 3;
    }
    baked(root);
    refusals(root);
    for (const char* f : {"/ui/touch.txt", "/game.manifest"}) platform::remove_file(root + f);
    std::printf("assetc-touch: %s - %d cases\n", failures == 0 ? "PASS" : "FAIL", cases);
    return failures == 0 ? 0 : 1;
}
