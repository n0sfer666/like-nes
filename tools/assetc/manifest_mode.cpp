#include "assetc_manifest_mode.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "assetc_depfile.hpp"
#include "assetc_emit.hpp"
#include "assetc_manifest.hpp"
#include "assetc_tool.hpp"
#include "manifest_bake.hpp"
#include "platform_path.hpp"

namespace asset::manifest {
namespace {

int usage(const std::string& tool) {
    std::fprintf(stderr, "usage: %s --manifest <file> <out.bundle> [--depfile <file.d>] [--basisu <path>]\n",
                 tool.c_str());
    return 2;
}

int unexpected(const std::string& tool, const char* bad) {
    std::fprintf(stderr, "%s --manifest: unexpected argument '%s'\n", tool.c_str(), bad);
    return usage(tool);
}

} // namespace

int run_manifest(int argc, char** argv, std::span<const Kind> kinds) {
    const std::string tool = tool_name(argc > 0 ? argv[0] : nullptr);
    if (argc < 4) return usage(tool);
    // Битая таблица видов — ошибка инструмента, а не манифеста: без пары `<манифест>:<строка>`.
    if (std::string why; !kinds_ok(kinds, why)) {
        std::fprintf(stderr, "[assetc] %s\n", why.c_str());
        return 1;
    }
    Bake b;
    b.kinds = kinds;
    b.manifest = argv[2];
    b.tools = codec::Tools{"tint", "basisu"};
    const std::string out = argv[3];
    b.tmp = out + ".ktx2.tmp";
    const char* depfile = nullptr;
    // Хвост по одному аргументу с отказом на неузнанном — тот же урок, что у `--materials`: флаг
    // без значения, проглоченный молча, оставил бы сборку без depfile и без перебейка.
    for (int i = 4; i < argc; ++i) {
        if (i + 1 >= argc) return unexpected(tool, argv[i]);
        if (std::strcmp(argv[i], "--depfile") == 0) depfile = argv[++i];
        else if (std::strcmp(argv[i], "--basisu") == 0) b.tools.basisu = argv[++i];
        else return unexpected(tool, argv[i]);
    }
    if (!bake(b)) {
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

} // namespace asset::manifest
