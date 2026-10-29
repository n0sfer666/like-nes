#include "assetc_game.hpp"

#include <cstdio>
#include <cstring>

#include "assetc_emit.hpp"
#include "assetc_tool.hpp"
#include "bakers.hpp"
#include "codec.hpp"

namespace asset::game {

// Игра-образец (спека #8 шов assetc→билд): плейсхолдер-atlas.png → target-native UASTC bake →
// game.bundle. Рантайм игры транскодит UASTC→BC7 и грузит из бандла (не из процедурного кода).
int run_game(int argc, char** argv, std::span<const Section> sections) {
    const std::string tool = tool_name(argv[0]);
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s --game <src-dir> <out.bundle> [--basisu P]\n", tool.c_str());
        return 2;
    }
    const std::string src = argv[2], out = argv[3];
    codec::Tools tools{"tint", "basisu"};
    for (int i = 4; i + 1 < argc; i += 2)
        if (std::strcmp(argv[i], "--basisu") == 0) tools.basisu = argv[i + 1];
    std::vector<AssetInput> assets;
    if (!bakers::texture(tools, src + "/atlas.png", "atlas", out + ".ktx2.tmp", assets)) return 1;
    for (const Section& s : sections)
        if (!s.bake(src + s.file, assets)) return 1;
    return emit("game", out, std::move(assets));
}

// Сверка закоммиченного бандла с исходниками. Отдельный режим, а не флаг бейка: перепечь `--game`
// без tint/basisu нельзя, а сверить tool-free секции можно везде.
int run_verify(int argc, char** argv, std::span<const Section> sections) {
    const std::string tool = tool_name(argv[0]);
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s --verify-game <src-dir> <bundle>\n", tool.c_str());
        return 2;
    }
    return verify_game_bundle(tool, argv[2], argv[3], sections) ? 0 : 1;
}

} // namespace asset::game
