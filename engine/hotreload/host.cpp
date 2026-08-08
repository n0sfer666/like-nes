#include "game_api.h"

#include <cstdio>

#include "platform_args.hpp"
#include "platform_guard.hpp"
#include "platform_module.hpp"

typedef void (*game_tick_fn)(GameState*);

// Крэш в gameplay изолируется швом platform_guard: siglongjmp на POSIX, SEH на Windows —
// эквивалент catch_unwind из ADR.
struct Lib {
    platform::Module mod;
    game_tick_fn tick = nullptr;
};

static Lib load(const char* path) {
    Lib lib;
    if (!lib.mod.open(path)) {
        std::fprintf(stderr, "load failed: %s\n", platform::Module::last_error());
        return lib;
    }
    lib.tick = reinterpret_cast<game_tick_fn>(lib.mod.symbol("game_tick"));
    if (!lib.tick) std::fprintf(stderr, "symbol failed: %s\n", platform::Module::last_error());
    return lib;
}

static void unload(Lib& lib) {
    lib.mod.close();
    lib.tick = nullptr;
}

namespace {
struct Tick {
    game_tick_fn fn;
    GameState* state;
};
void run_tick(void* p) {
    auto* t = static_cast<Tick*>(p);
    t->fn(t->state);
}
} // namespace

// Возврат: true = отработало, false = пойман крэш (host выжил).
static bool safe_tick(game_tick_fn fn, GameState* s) {
    Tick t{fn, s};
    return platform::guarded_call(run_tick, &t);
}

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 4) {
        std::fprintf(stderr, "usage: host <game_a> <game_b> <game_crash>\n");
        return 2;
    }

    platform::install_crash_isolation();

    GameState state{}; // host-owned — переживает все reload

    // 1) Загрузка версии A, 3 тика (+1 каждый).
    Lib a = load(argv[1]);
    if (!a.tick) return 1;
    for (int i = 0; i < 3; ++i) safe_tick(a.tick, &state);
    std::printf("[host] after A x3:        x=%d ticks=%d\n", state.x.to_int(), state.ticks);
    unload(a);

    // 2) Hot-reload на версию B — state НЕ трогаем (живёт в host). 3 тика (+10 каждый).
    Lib b = load(argv[2]);
    if (!b.tick) return 1;
    for (int i = 0; i < 3; ++i) safe_tick(b.tick, &state);
    std::printf("[host] after reload->B x3: x=%d ticks=%d (state kept, behaviour changed)\n",
                state.x.to_int(), state.ticks);
    unload(b);

    // 3) Крэш-изоляция: версия с null-deref. Host должен выжить.
    Lib c = load(argv[3]);
    if (!c.tick) return 1;
    bool ok = safe_tick(c.tick, &state);
    std::printf("[host] crash tick:         %s (ticks=%d)\n",
                ok ? "OK" : "CAUGHT/isolated", state.ticks);
    unload(c);

    std::printf("[host] SURVIVED:           x=%d ticks=%d\n", state.x.to_int(), state.ticks);

    bool pass = (state.x.to_int() == 33) && (state.ticks == 7) && !ok;
    std::printf("[host] hot-reload PoC: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
