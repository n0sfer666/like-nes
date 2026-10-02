#include "manifest_mode.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "assetc_depfile.hpp"
#include "assetc_emit.hpp"
#include "manifest_bake.hpp"
#include "platform_path.hpp"

namespace asset {
namespace {

constexpr const char* USAGE =
    "usage: assetc --manifest <file> <out.bundle> [--depfile <file.d>] [--basisu <path>]\n";

int usage(const char* bad) {
    std::fprintf(stderr, "assetc --manifest: unexpected argument '%s'\n", bad);
    std::fputs(USAGE, stderr);
    return 2;
}

} // namespace

int run_manifest(int argc, char** argv) {
    if (argc < 4) {
        std::fputs(USAGE, stderr);
        return 2;
    }
    manifest::Bake b;
    b.manifest = argv[2];
    b.tools = codec::Tools{"tint", "basisu"};
    const std::string out = argv[3];
    b.tmp = out + ".ktx2.tmp";
    const char* depfile = nullptr;
    // Хвост по одному аргументу с отказом на неузнанном — тот же урок, что у `--materials`: флаг
    // без значения, проглоченный молча, оставил бы сборку без depfile и без перебейка.
    for (int i = 4; i < argc; ++i) {
        if (i + 1 >= argc) return usage(argv[i]);
        if (std::strcmp(argv[i], "--depfile") == 0) depfile = argv[++i];
        else if (std::strcmp(argv[i], "--basisu") == 0) b.tools.basisu = argv[++i];
        else return usage(argv[i]);
    }
    if (!manifest::bake(b)) {
        std::fprintf(stderr, "[assetc] %s\n", b.error.c_str());
        return 1;
    }
    if (const int rc = emit("manifest", out, std::move(b.assets)); rc != 0) return rc;
    if (!depfile) return 0;
    const std::string text = depfile::text(out, b.deps, platform::is_sep('\\'));
    if (!codec::write_file(depfile, std::vector<uint8_t>(text.begin(), text.end()))) {
        std::fprintf(stderr, "[assetc] write failed: %s\n", depfile);
        return 1;
    }
    return 0;
}

} // namespace asset
