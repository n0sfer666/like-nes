#include <cstdio>
#include <cstring>

#include "assetc_manifest_mode.hpp"
#include "fighter_kind.hpp"
#include "platform_args.hpp"

// Пекарь Neon Rumble (спека #29, В2а): ядро пекаря SDK + вид записи `fighter`.
int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc >= 2 && std::strcmp(argv[1], "--manifest") == 0)
        return asset::manifest::run_manifest(argc, argv, rumble::assetc_kinds());
    std::fprintf(stderr, "usage: rumble_assetc --manifest <game.manifest> <out.bundle> [--depfile D] [--basisu P]\n");
    return 2;
}
