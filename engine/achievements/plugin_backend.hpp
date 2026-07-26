#pragma once
#include "backend.hpp"

struct AchBackendApi;
class Registry;

namespace ach {

// Адаптер C-ABI бэкенда из плагина (#6, EXT_ACHIEVEMENT_BACKEND) к движковому интерфейсу.
// Отсутствующие указатели трактуются как «нечего делать» — кривой плагин не роняет хост.
class PluginBackend final : public Backend {
public:
    explicit PluginBackend(const AchBackendApi* api) : api_(api) {}

    bool begin() override;
    void declare(const char* key) override;
    Send unlock(const char* key) override;
    Send set_stat(const char* key, uint64_t value) override;
    Send commit() override;
    int32_t poll_remote(const char** out_keys, int32_t cap) override;
    void end() override;

private:
    const AchBackendApi* api_;
};

const AchBackendApi* find_backend(const ::Registry& reg, const char* id = nullptr);

} // namespace ach
