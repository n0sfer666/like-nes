#include "plugin_backend.hpp"

#include "../plugin/registry.hpp"

namespace ach {
namespace {

Send from_code(int32_t code) {
    if (code == ACH_SEND_OK) return Send::Ok;
    if (code == ACH_SEND_RETRY) return Send::Retry;
    return Send::Fatal;
}

} // namespace

bool PluginBackend::begin() {
    if (api_ == nullptr) return false;
    if (api_->begin == nullptr) return true;
    return api_->begin(api_->self) != 0;
}

void PluginBackend::declare(const char* key) {
    if (api_ != nullptr && api_->declare != nullptr) api_->declare(api_->self, key);
}

Send PluginBackend::unlock(const char* key) {
    if (api_ == nullptr || api_->unlock == nullptr) return Send::Ok;
    return from_code(api_->unlock(api_->self, key));
}

Send PluginBackend::set_stat(const char* key, uint64_t value) {
    if (api_ == nullptr || api_->set_stat == nullptr) return Send::Ok;
    return from_code(api_->set_stat(api_->self, key, value));
}

Send PluginBackend::commit() {
    if (api_ == nullptr || api_->commit == nullptr) return Send::Ok;
    return from_code(api_->commit(api_->self));
}

int32_t PluginBackend::poll_remote(const char** out_keys, int32_t cap) {
    if (api_ == nullptr || api_->poll_remote == nullptr) return -1;
    return api_->poll_remote(api_->self, out_keys, cap);
}

void PluginBackend::end() {
    if (api_ != nullptr && api_->end != nullptr) api_->end(api_->self);
}

const AchBackendApi* find_backend(const ::Registry& reg, const char* id) {
    for (const BackendExt& e : reg.backends()) {
        if (id != nullptr && e.id != id) continue;
        if (e.api == nullptr) continue;
        return e.api;
    }
    return nullptr;
}

} // namespace ach
