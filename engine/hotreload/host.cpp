#include "game_api.h"

#include <dlfcn.h>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstring>

typedef void (*game_tick_fn)(GameState*);

static sigjmp_buf g_jmp;

// Крэш в gameplay ловится сигнал-хендлером и возвращает управление в host через siglongjmp
// (эквивалент catch_unwind из ADR, *nix-путь; на Windows это будет SEH).
static void crash_handler(int sig) { siglongjmp(g_jmp, sig); }

struct Lib {
    void* handle = nullptr;
    game_tick_fn tick = nullptr;
};

static Lib load(const char* path) {
    Lib lib;
    lib.handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib.handle) { std::fprintf(stderr, "dlopen failed: %s\n", dlerror()); return lib; }
    lib.tick = reinterpret_cast<game_tick_fn>(dlsym(lib.handle, "game_tick"));
    if (!lib.tick) std::fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return lib;
}

static void unload(Lib& lib) {
    if (lib.handle) dlclose(lib.handle);
    lib.handle = nullptr;
    lib.tick = nullptr;
}

// Возврат: true = отработало, false = пойман крэш (host выжил).
static bool safe_tick(game_tick_fn fn, GameState* s) {
    if (sigsetjmp(g_jmp, 1) == 0) { fn(s); return true; }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: host <game_a.dylib> <game_b.dylib> <game_crash.dylib>\n");
        return 2;
    }

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);

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
    std::printf("[host] after reload->B x3: x=%d ticks=%d (state сохранён, поведение сменилось)\n",
                state.x.to_int(), state.ticks);
    unload(b);

    // 3) Крэш-изоляция: версия с null-deref. Host должен выжить.
    Lib c = load(argv[3]);
    if (!c.tick) return 1;
    bool ok = safe_tick(c.tick, &state);
    std::printf("[host] crash tick:         %s (ticks=%d)\n",
                ok ? "OK" : "ПОЙМАН/изолирован", state.ticks);
    unload(c);

    std::printf("[host] SURVIVED:           x=%d ticks=%d\n", state.x.to_int(), state.ticks);

    bool pass = (state.x.to_int() == 33) && (state.ticks == 7) && !ok;
    std::printf("[host] hot-reload PoC: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
