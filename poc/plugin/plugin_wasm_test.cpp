#include "wasm_host.hpp"
#include "registry.hpp"
#include "builtin.hpp"
#include "host.hpp"
#include "sim.hpp"
#include <cstdio>
#include <string>

static const char* OOB_WAT =
    "(module (memory (export \"mem\") 1)"
    " (func (export \"boom\") (i32.store (i32.const 100000000) (i32.const 1))))";

static const char* CAP_WAT =
    "(module (import \"host\" \"secret\" (func $s))"
    " (func (export \"go\") (call $s)))";

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: plugin_wasm_test <gravity.wat> <wind.so>\n");
        return 2;
    }
    const std::string wat = argv[1];
    const std::string wind = argv[2];
    const int TICKS = 3000;
    const uint64_t GOLDEN = 0x7ad0493f0f2ddf47ull;

    Registry reg;
    PluginHost host(reg);
    reg.set_current_owner("builtin");
    reg.add_ecs_system("integrate", {"gravity", "wind"}, sys_integrate);
    reg.add_ecs_system("bounce", {"integrate"}, sys_bounce);
    reg.set_current_owner("");
    if (!host.load_native(wind)) { std::fprintf(stderr, "cannot load wind\n"); return 1; }
    bool sok = true;
    auto sched = reg.schedule(&sok);

    WasmGravity g;
    if (!g.init(wat)) { std::fprintf(stderr, "wasm init: %s\n", g.error().c_str()); return 1; }

    const int32_t g_raw = fix32::from_float(9.8).raw;
    const int32_t dt_raw = fix32::from_float(1.0 / 60.0).raw;

    SimWorld w;
    sim_init(w);
    for (int t = 0; t < TICKS; ++t) {
        if (!g.apply(w, g_raw, dt_raw)) { std::fprintf(stderr, "wasm apply: %s\n", g.error().c_str()); return 1; }
        for (const auto& s : sched) s.fn(&w);
        w.tick++;
    }
    uint64_t h_wasm = sim_hash(w);

    WasmOutcome oob = wasm_run_escape(OOB_WAT, "boom");
    WasmOutcome cap = wasm_run_escape(CAP_WAT, "go");

    std::printf("[plugin-wasm] native golden = 0x%016llx\n", (unsigned long long)GOLDEN);
    std::printf("[plugin-wasm] WASM golden   = 0x%016llx\n", (unsigned long long)h_wasm);
    std::printf("[plugin-wasm] native == WASM (bit-exact fix32 across ABI): %s\n",
                h_wasm == GOLDEN ? "YES" : "NO");
    std::printf("[plugin-wasm] escape OOB linear-memory -> trap (host alive): %s\n",
                oob == WasmOutcome::TrapOnCall ? "YES" : "NO");
    std::printf("[plugin-wasm] escape ungranted host import -> link rejected: %s\n",
                cap == WasmOutcome::LinkError ? "YES" : "NO");

    bool pass = sok && (h_wasm == GOLDEN)
                && (oob == WasmOutcome::TrapOnCall)
                && (cap == WasmOutcome::LinkError);
    std::printf("plugin-wasm: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
