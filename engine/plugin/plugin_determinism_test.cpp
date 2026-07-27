#include "registry.hpp"
#include "runner.hpp"
#include "builtin.hpp"
#include "host.hpp"
#include "platform_args.hpp"
#include <cstdio>
#include <string>
#include <vector>

static void add_builtins(Registry& reg) {
    reg.set_current_owner("builtin");
    reg.add_ecs_system("integrate", {"gravity", "wind"}, sys_integrate);
    reg.add_ecs_system("bounce", {"integrate"}, sys_bounce);
    reg.set_current_owner("");
}

static uint64_t run_with(const std::vector<std::string>& paths, int ticks, bool* ok) {
    Registry reg;
    PluginHost host(reg);
    add_builtins(reg);
    for (const auto& p : paths) {
        if (!host.load_native(p)) { *ok = false; return 0; }
    }
    bool s = true;
    uint64_t h = run_sim(reg, ticks, &s);
    if (!s) *ok = false;
    return h;
}

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "usage: plugin_determinism_test <gravity.so> <wind.so>\n");
        return 2;
    }
    const std::string g = argv[1];
    const std::string w = argv[2];
    const int TICKS = 3000;

    bool ok = true;
    uint64_t h_order1 = run_with({g, w}, TICKS, &ok);
    uint64_t h_order2 = run_with({w, g}, TICKS, &ok);
    uint64_t h_rerun = run_with({g, w}, TICKS, &ok);

    Registry base;
    add_builtins(base);
    bool bok = true;
    uint64_t h_without = run_sim(base, TICKS, &bok);

    std::printf("[plugin-determinism] H_with     = 0x%016llx\n", (unsigned long long)h_order1);
    std::printf("[plugin-determinism] H_without  = 0x%016llx\n", (unsigned long long)h_without);
    std::printf("[plugin-determinism] order-independent (loadA==loadB): %s\n",
                h_order1 == h_order2 ? "YES" : "NO");
    std::printf("[plugin-determinism] run-to-run stable:                %s\n",
                h_order1 == h_rerun ? "YES" : "NO");
    std::printf("[plugin-determinism] plugin affects sim (catches regression): %s\n",
                h_order1 != h_without ? "YES" : "NO");

    bool pass = ok && bok
                && (h_order1 == h_order2)
                && (h_order1 == h_rerun)
                && (h_order1 != h_without);
    std::printf("plugin-determinism: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
