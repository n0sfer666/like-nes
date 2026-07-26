#include "backend_host.hpp"

#include "../engine/achievements/plugin_backend.hpp"
#include "../engine/plugin/host.hpp"

namespace game {

struct BackendHost::Impl {
    ::Registry reg;
    std::unique_ptr<PluginHost> host;
    std::unique_ptr<ach::PluginBackend> backend;
    std::string path;
};

BackendHost::BackendHost() : impl_(new Impl()) {}

BackendHost::~BackendHost() {
    impl_->backend.reset();
    if (impl_->host && !impl_->path.empty()) impl_->host->unload(impl_->path);
}

// Порядок разрушения тут load-bearing: backend указывает внутрь библиотеки, которую dlclose-ит
// PluginHost, поэтому старый backend умирает ДО подмены хоста, а на пути отказа не остаётся вовсе.
ach::Backend* BackendHost::load(const std::string& plugin_path) {
    impl_->backend.reset();
    if (impl_->host && !impl_->path.empty()) impl_->host->unload(impl_->path);
    impl_->path.clear();
    impl_->host.reset();
    if (plugin_path.empty()) return nullptr;
    impl_->host.reset(new PluginHost(impl_->reg));
    if (!impl_->host->load_native(plugin_path)) {
        impl_->host.reset();
        return nullptr;
    }
    // Плагин без бэкенда выгружается тут же: иначе load() вернул бы nullptr, а библиотека осталась
    // бы в памяти до конца процесса — состояние «хост есть, бэкенда нет» никто не наблюдает.
    const AchBackendApi* api = ach::find_backend(impl_->reg);
    if (api == nullptr) {
        impl_->host->unload(plugin_path);
        impl_->host.reset();
        return nullptr;
    }
    impl_->path = plugin_path;
    impl_->backend.reset(new ach::PluginBackend(api));
    return impl_->backend.get();
}

} // namespace game
