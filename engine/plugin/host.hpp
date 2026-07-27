#pragma once
#include "platform_module.hpp"
#include "registry.hpp"
#include "sim.hpp"
#include <string>
#include <vector>
#include <map>

class PluginHost {
public:
    explicit PluginHost(Registry& reg) : reg_(reg) {}
    ~PluginHost();

    bool load_native(const std::string& path);
    void unload(const std::string& path);
    bool reload(const std::string& path);

    // Символ уже загруженного модуля. Заглянуть внутрь плагина, открыв его вторым Module'ом,
    // переносимо НЕЛЬЗЯ: на Windows каждое открытие грузит свою копию, и статика у копии своя
    // (см. platform_module.hpp). Спрашивать надо у того, кто модуль держит.
    void* symbol(const std::string& path, const char* name) const;

    std::vector<std::string> probe_and_disable_crashers();

    const std::vector<std::string>& failed() const { return failed_; }

private:
    Registry& reg_;
    std::map<std::string, platform::Module> modules_;
    std::vector<std::string> failed_;
};

bool safe_call_system(SimSystemFn fn, SimWorld* w);
void install_crash_isolation();
