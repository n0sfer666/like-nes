#include "host.hpp"
#include <cstdio>

#include "platform_guard.hpp"
#include "platform_module.hpp"

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define PLUGIN_ASAN 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define PLUGIN_ASAN 1
#endif
// Под ASan выгрузка модуля выключена, чтобы санитайзер мог разрешать символы выгруженных
// плагинов (крэш-изоляция #6). Сценариям, которые проверяют жизнь ПОСЛЕ выгрузки, нужна
// настоящая выгрузка — они собираются с -DPLUGIN_FORCE_DLCLOSE и платят нечитаемыми стеками.
#if defined(PLUGIN_FORCE_DLCLOSE) && defined(PLUGIN_ASAN)
#  undef PLUGIN_ASAN
#endif

namespace {

void drop(platform::Module& m) {
#ifndef PLUGIN_ASAN
    m.close();
#else
    m.detach();
#endif
}

struct Invocation {
    SimSystemFn fn;
    SimWorld* w;
};

void invoke(void* p) {
    auto* c = static_cast<Invocation*>(p);
    c->fn(c->w);
}

} // namespace

void install_crash_isolation() { platform::install_crash_isolation(); }

bool safe_call_system(SimSystemFn fn, SimWorld* w) {
    Invocation call{fn, w};
    return platform::guarded_call(invoke, &call);
}

PluginHost::~PluginHost() {
    for (auto& [path, m] : modules_) drop(m);
}

bool PluginHost::load_native(const std::string& path) {
    if (modules_.count(path)) unload(path);
    platform::Module m;
    if (!m.open(path)) {
        std::fprintf(stderr, "[host] load failed: %s\n", platform::Module::last_error());
        return false;
    }
    auto abi = reinterpret_cast<PluginAbiFn>(m.symbol("plugin_abi_version"));
    if (!abi || abi() != PLUGIN_API_VERSION) {
        std::fprintf(stderr, "[host] ABI mismatch in %s (got=%d want=%d)\n",
                     path.c_str(), abi ? abi() : -1, PLUGIN_API_VERSION);
        drop(m);
        return false;
    }
    auto entry = reinterpret_cast<PluginMainFn>(m.symbol("plugin_main"));
    if (!entry) {
        std::fprintf(stderr, "[host] no plugin_main in %s\n", path.c_str());
        drop(m);
        return false;
    }
    reg_.set_current_owner(path);
    HostApi api = reg_.make_host_api();
    entry(&api);
    reg_.set_current_owner("");
    modules_[path] = std::move(m);
    return true;
}

void* PluginHost::symbol(const std::string& path, const char* name) const {
    const auto it = modules_.find(path);
    return it == modules_.end() ? nullptr : it->second.symbol(name);
}

void PluginHost::unload(const std::string& path) {
    reg_.remove_owner(path);
    auto it = modules_.find(path);
    if (it != modules_.end()) {
        drop(it->second);
        modules_.erase(it);
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
