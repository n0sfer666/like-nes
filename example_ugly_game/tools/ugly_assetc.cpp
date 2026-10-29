#include <cstdio>
#include <cstring>

#include "assetc_game.hpp"
#include "bakers.hpp"
#include "movement_section.hpp"
#include "platform_args.hpp"

// Пекарь игры-образца (спека #29, В2а): ядро `assetc_core` + таблица секций `game.bundle`.
namespace {

const asset::game::Section SECTIONS[] = {
    {"input presets", "/input.txt", asset::bakers::input_presets},
    {"achievements", "/achievements.txt", asset::bakers::achievements},
    {"movement", "/movement.txt", game::movement_section},
    {"tilemap", "/tilemap.txt", asset::bakers::tilemap},
    {"atlas regions", "/atlas.txt", asset::bakers::atlas_regions},
    {"touch layout", "/touch.txt", asset::bakers::touch_layout},
};

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc >= 2 && std::strcmp(argv[1], "--game") == 0) return asset::game::run_game(argc, argv, SECTIONS);
    if (argc >= 2 && std::strcmp(argv[1], "--verify-game") == 0)
        return asset::game::run_verify(argc, argv, SECTIONS);
    std::fprintf(stderr,
                 "usage: ugly_assetc --game <src-dir> <out.bundle> [--basisu P]\n"
                 "       ugly_assetc --verify-game <src-dir> <bundle>  (tools-free, for CI)\n");
    return 2;
}
