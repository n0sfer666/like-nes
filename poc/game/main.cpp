#include <cstdlib>
#include <cstring>

#include "app.hpp"

int main(int argc, char** argv) {
    const char* demo_dir = nullptr;
    int frames = 300, cap = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--demo") && i + 1 < argc) demo_dir = argv[++i];
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = cap = std::atoi(argv[++i]);
    }
    if (demo_dir) return game::run_demo(demo_dir, frames);
    return game::run_window(cap);
}
