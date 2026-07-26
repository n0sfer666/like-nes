#include "host.hpp"
#include <cstdio>
#include <cstring>

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define PLUGIN_ASAN 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define PLUGIN_ASAN 1
#endif
// Под ASan dlclose выключен, чтобы санитайзер мог разрешать символы выгруженных плагинов
// (крэш-изоляция #6). Сценариям, которые проверяют жизнь ПОСЛЕ выгрузки, нужен настоящий
// dlclose — они собираются с -DPLUGIN_FORCE_DLCLOSE и платят нечитаемыми стеками.
#if defined(PLUGIN_FORCE_DLCLOSE) && defined(PLUGIN_ASAN)
#  undef PLUGIN_ASAN
#endif

#ifdef _WIN32
#include <windows.h>
static void* dl_open(const char* p) { return (void*)LoadLibraryA(p); }
static void* dl_sym(void* h, const char* s) { return (void*)GetProcAddress((HMODULE)h, s); }
static void dl_close(void* h) { FreeLibrary((HMODULE)h); }
static const char* dl_err() { return "LoadLibrary failed"; }
#else
#include <dlfcn.h>
static void* dl_open(const char* p) { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
static void* dl_sym(void* h, const char* s) { return dlsym(h, s); }
static void dl_close(void* h) {
#ifndef PLUGIN_ASAN
    dlclose(h);
#else
    (void)h;
#endif
}
static const char* dl_err() { return dlerror(); }
#endif

#ifndef _WIN32
#include <csetjmp>
#include <csignal>

static sigjmp_buf g_plugin_jmp;
static volatile sig_atomic_t g_installed = 0;
static volatile sig_atomic_t g_armed = 0;

static void plugin_crash_handler(int sig) {
    if (!g_armed) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
        return;
    }
    g_armed = 0;
    siglongjmp(g_plugin_jmp, sig);
}

void install_crash_isolation() {
    if (g_installed) return;
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = plugin_crash_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    g_installed = 1;
}

bool safe_call_system(SimSystemFn fn, SimWorld* w) {
    install_crash_isolation();
    if (sigsetjmp(g_plugin_jmp, 1) == 0) {
        g_armed = 1;
        fn(w);
        g_armed = 0;
        return true;
    }
    g_armed = 0;
    return false;
}
#else
void install_crash_isolation() {}
bool safe_call_system(SimSystemFn fn, SimWorld* w) { fn(w); return true; }
#endif

PluginHost::~PluginHost() {
    for (auto& [path, h] : handles_) if (h) dl_close(h);
}

bool PluginHost::load_native(const std::string& path) {
    if (handles_.count(path)) unload(path);
    void* h = dl_open(path.c_str());
    if (!h) { std::fprintf(stderr, "[host] load failed: %s\n", dl_err()); return false; }
    auto abi = reinterpret_cast<PluginAbiFn>(dl_sym(h, "plugin_abi_version"));
    if (!abi || abi() != PLUGIN_API_VERSION) {
        std::fprintf(stderr, "[host] ABI mismatch in %s (got=%d want=%d)\n",
                     path.c_str(), abi ? abi() : -1, PLUGIN_API_VERSION);
        dl_close(h);
        return false;
    }
    auto entry = reinterpret_cast<PluginMainFn>(dl_sym(h, "plugin_main"));
    if (!entry) { std::fprintf(stderr, "[host] no plugin_main in %s\n", path.c_str()); dl_close(h); return false; }
    reg_.set_current_owner(path);
    HostApi api = reg_.make_host_api();
    entry(&api);
    reg_.set_current_owner("");
    handles_[path] = h;
    return true;
}

void PluginHost::unload(const std::string& path) {
    reg_.remove_owner(path);
    auto it = handles_.find(path);
    if (it != handles_.end()) {
        if (it->second) dl_close(it->second);
        handles_.erase(it);
    }
}

bool PluginHost::reload(const std::string& path) {
    unload(path);
    return load_native(path);
}

std::vector<std::string> PluginHost::probe_and_disable_crashers() {
    std::vector<std::string> disabled;
    bool changed = true;
    while (changed) {
        changed = false;
        bool ok = false;
        auto sched = reg_.schedule(&ok);
        for (const auto& s : sched) {
            SimWorld scratch;
            sim_init(scratch);
            if (!safe_call_system(s.fn, &scratch)) {
                std::fprintf(stderr, "[host] plugin system '%s' (owner=%s) crashed on probe -> disabled\n",
                             s.id.c_str(), s.owner.c_str());
                failed_.push_back(s.owner);
                disabled.push_back(s.owner);
                unload(s.owner);
                changed = true;
                break;
            }
        }
    }
    return disabled;
}
