#include "registry.hpp"
#include "runner.hpp"
#include "builtin.hpp"
#include "host.hpp"
#include "platform_args.hpp"
#include <algorithm>
#include <cstdio>
#include <string>

static void add_builtins(Registry& reg) {
    reg.set_current_owner("builtin");
    reg.add_ecs_system("integrate", {"gravity", "wind"}, sys_integrate);
    reg.add_ecs_system("bounce", {"integrate"}, sys_bounce);
    reg.set_current_owner("");
}

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 4) {
        std::fprintf(stderr, "usage: plugin_isolation_test <gravity.so> <wind.so> <crash.so>\n");
        return 2;
    }
    const std::string g = argv[1], w = argv[2], c = argv[3];
    const int TICKS = 3000;

    uint64_t h_clean;
    {
        Registry reg;
        PluginHost host(reg);
        add_builtins(reg);
        host.load_native(g);
        host.load_native(w);
        bool ok = true;
        h_clean = run_sim(reg, TICKS, &ok);
    }

    Registry reg;
    PluginHost host(reg);
    add_builtins(reg);
    host.load_native(g);
    host.load_native(w);
    bool loaded_crash = host.load_native(c);

    auto disabled = host.probe_and_disable_crashers();
    bool crash_disabled = std::find(disabled.begin(), disabled.end(), c) != disabled.end();
    bool survivors_ok = (reg.find_codec("nope") == nullptr);
    size_t ecs_left = reg.count(EXT_ECS_SYSTEM);

    bool ok = true;
    uint64_t h_after = run_sim(reg, TICKS, &ok);

    SimWorld world;
    sim_init(world);
    bool sok = true;
    auto sched = reg.schedule(&sok);
    for (int t = 0; t < 100; ++t) { for (const auto& s : sched) s.fn(&world); world.tick++; }
    uint64_t h_before_reload = sim_hash(world);
    host.reload(g);
    uint64_t h_after_reload = sim_hash(world);
    sched = reg.schedule(&sok);
    for (int t = 100; t < 200; ++t) { for (const auto& s : sched) s.fn(&world); world.tick++; }
    size_t ecs_after_reload = reg.count(EXT_ECS_SYSTEM);

    host.load_native(c);
    auto disabled2 = host.probe_and_disable_crashers();
    bool crash_recaught = std::find(disabled2.begin(), disabled2.end(), c) != disabled2.end();
    size_t ecs_final = reg.count(EXT_ECS_SYSTEM);

    std::printf("[plugin-isolation] crash loaded:            %s\n", loaded_crash ? "YES" : "NO");
    std::printf("[plugin-isolation] crash caught & disabled: %s\n", crash_disabled ? "YES" : "NO");
    std::printf("[plugin-isolation] host survived probe:     YES\n");
    std::printf("[plugin-isolation] ecs systems after disable: %zu (integrate,bounce,gravity,wind)\n", ecs_left);
    std::printf("[plugin-isolation] sim clean after disable (==no-crash hash): %s\n",
                h_after == h_clean ? "YES" : "NO");
    std::printf("[plugin-isolation] hot-reload: host-owned state survived swap: %s\n",
                h_before_reload == h_after_reload ? "YES" : "NO");
    std::printf("[plugin-isolation] ecs systems after reload: %zu\n", ecs_after_reload);
    std::printf("[plugin-isolation] re-probe catches re-loaded crash (armed re-arms): %s\n",
                crash_recaught ? "YES" : "NO");
    std::printf("[plugin-isolation] ecs systems final:       %zu\n", ecs_final);

    bool pass = loaded_crash && crash_disabled && survivors_ok
                && (ecs_left == 4)
                && (h_after == h_clean)
                && (h_before_reload == h_after_reload)
                && (ecs_after_reload == 4)
                && crash_recaught && (ecs_final == 4)
                && ok && sok;
    std::printf("plugin-isolation: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
