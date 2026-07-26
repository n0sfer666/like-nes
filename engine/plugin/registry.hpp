#pragma once
#include "plugin_api.h"
#include <string>
#include <vector>

struct EcsSystem {
    std::string id;
    std::vector<std::string> after;
    SimSystemFn fn;
    std::string owner;
};

struct AssetCodec {
    std::string fourcc;
    AssetDecodeFn fn;
    std::string owner;
};

struct NamedExt {
    std::string id;
    std::string extra;
    void* fn;
    std::string owner;
};

struct BackendExt {
    std::string id;
    const AchBackendApi* api;
    std::string owner;
};

struct ScheduledSystem {
    std::string id;
    SimSystemFn fn;
    std::string owner;
};

// У ECS-систем, кодеков и бэкендов достижений своё хранилище — named-слот по такому виду молча
// остался бы пустым, хотя count() считает его по-настоящему. Предикат один на все точки входа.
constexpr bool ext_has_own_storage(ExtKind kind) {
    return kind == EXT_ECS_SYSTEM || kind == EXT_ASSET_CODEC || kind == EXT_ACHIEVEMENT_BACKEND;
}

constexpr bool ext_in_range(ExtKind kind) {
    return static_cast<int>(kind) >= 0 && static_cast<int>(kind) < EXT_KIND_COUNT;
}

class Registry {
public:
    void set_current_owner(const std::string& owner) { current_owner_ = owner; }

    void add_ecs_system(const std::string& id, std::vector<std::string> after, SimSystemFn fn);
    void add_asset_codec(const std::string& fourcc, AssetDecodeFn fn);
    void add_named(ExtKind kind, const std::string& id, const std::string& extra, void* fn);
    void add_backend(const std::string& id, const AchBackendApi* backend);

    void remove_owner(const std::string& owner);

    std::vector<ScheduledSystem> schedule(bool* ok) const;
    AssetDecodeFn find_codec(const std::string& fourcc) const;

    const std::vector<EcsSystem>& ecs_systems() const { return ecs_; }
    // Вид задаётся типом, а не значением: named(EXT_ACHIEVEMENT_BACKEND) молча отдавал бы пустоту,
    // хотя count() по тому же виду считает backends_.
    template <ExtKind K>
    const std::vector<NamedExt>& named() const {
        static_assert(ext_in_range(K) && !ext_has_own_storage(K),
                      "у этого вида расширений собственное хранилище");
        return named_[K];
    }
    const std::vector<BackendExt>& backends() const { return backends_; }
    std::size_t count(ExtKind kind) const;

    HostApi make_host_api();

private:
    std::string current_owner_;
    std::vector<EcsSystem> ecs_;
    std::vector<AssetCodec> codecs_;
    std::vector<NamedExt> named_[EXT_KIND_COUNT];
    std::vector<BackendExt> backends_;
};
