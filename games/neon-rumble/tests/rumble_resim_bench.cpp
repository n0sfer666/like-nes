#include <cstdio>
#include <cstring>
#include <string>

#include "platform_args.hpp"
#include "resim_bench.hpp"

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string against;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--against") != 0) continue;
        if (i + 1 == argc) {
            std::fprintf(stderr, "brawl-resim-bench: --against needs a crossplay file\n");
            return 2;
        }
        against = argv[++i];
    }
    return framework::ai::bench::run_resim_bench(against);
}
