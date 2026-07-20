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

struct ScheduledSystem {
    std::string id;
    SimSystemFn fn;
    std::string owner;
};

class Registry {
public:
    void set_current_owner(const std::string& owner) { current_owner_ = owner; }

    void add_ecs_system(const std::string& id, std::vector<std::string> after, SimSystemFn fn);
    void add_asset_codec(const std::string& fourcc, AssetDecodeFn fn);
    void add_named(ExtKind kind, const std::string& id, const std::string& extra, void* fn);

    void remove_owner(const std::string& owner);

    std::vector<ScheduledSystem> schedule(bool* ok) const;
    AssetDecodeFn find_codec(const std::string& fourcc) const;

    const std::vector<EcsSystem>& ecs_systems() const { return ecs_; }
    const std::vector<NamedExt>& named(ExtKind kind) const { return named_[kind]; }
    std::size_t count(ExtKind kind) const;

    HostApi make_host_api();

private:
    std::string current_owner_;
    std::vector<EcsSystem> ecs_;
    std::vector<AssetCodec> codecs_;
    std::vector<NamedExt> named_[6];
};
