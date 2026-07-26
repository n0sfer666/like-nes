#pragma once
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

    std::vector<std::string> probe_and_disable_crashers();

    const std::vector<std::string>& failed() const { return failed_; }

private:
    Registry& reg_;
    std::map<std::string, void*> handles_;
    std::vector<std::string> failed_;
};

bool safe_call_system(SimSystemFn fn, SimWorld* w);
void install_crash_isolation();
