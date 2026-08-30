#include <cstdlib>
#include <cstring>

#include "app.hpp"
#include "platform_args.hpp"

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    game::DemoOptions demo;
    bool headless = false;
    int cap = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--demo") && i + 1 < argc) { demo.dir = argv[++i]; headless = true; }
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc)
            demo.frames = cap = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--golden") && i + 1 < argc) { demo.golden = argv[++i]; headless = true; }
        else if (!std::strcmp(argv[i], "--update-golden")) { demo.update = true; headless = true; }
        else if (!std::strcmp(argv[i], "--golden-selftest")) { demo.selftest = true; headless = true; }
    }
    if (headless) return game::run_demo(demo);
    return game::run_window(cap);
}
