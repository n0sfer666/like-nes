#include "registry.hpp"
#include <algorithm>
#include <cstdio>

void Registry::add_ecs_system(const std::string& id, std::vector<std::string> after, SimSystemFn fn) {
    for (const auto& s : ecs_) {
        if (s.id == id) {
            std::fprintf(stderr, "[host] duplicate ecs-system id '%s' (owner=%s) rejected (already owned by %s)\n",
                         id.c_str(), current_owner_.c_str(), s.owner.c_str());
            return;
        }
    }
    ecs_.push_back(EcsSystem{id, std::move(after), fn, current_owner_});
}

void Registry::add_asset_codec(const std::string& fourcc, AssetDecodeFn fn) {
    codecs_.push_back(AssetCodec{fourcc, fn, current_owner_});
}

void Registry::add_named(ExtKind kind, const std::string& id, const std::string& extra, void* fn) {
    named_[kind].push_back(NamedExt{id, extra, fn, current_owner_});
}

void Registry::remove_owner(const std::string& owner) {
    auto drop = [&](auto& v) {
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](const auto& e) { return e.owner == owner; }),
                v.end());
    };
    drop(ecs_);
    drop(codecs_);
    for (auto& v : named_) drop(v);
}

std::vector<ScheduledSystem> Registry::schedule(bool* ok) const {
    std::vector<const EcsSystem*> remaining;
    remaining.reserve(ecs_.size());
    for (const auto& s : ecs_) remaining.push_back(&s);

    auto present = [&](const std::string& id) {
        for (const auto& s : ecs_) if (s.id == id) return true;
        return false;
    };

    std::vector<ScheduledSystem> out;
    std::vector<std::string> placed;
    auto is_placed = [&](const std::string& id) {
        return std::find(placed.begin(), placed.end(), id) != placed.end();
    };

    while (!remaining.empty()) {
        const EcsSystem* pick = nullptr;
        for (const EcsSystem* s : remaining) {
            bool ready = true;
            for (const auto& dep : s->after) {
                if (present(dep) && !is_placed(dep)) { ready = false; break; }
            }
            if (!ready) continue;
            if (!pick || s->id < pick->id) pick = s;
        }
        if (!pick) { if (ok) *ok = false; return out; }
        out.push_back(ScheduledSystem{pick->id, pick->fn, pick->owner});
        placed.push_back(pick->id);
        remaining.erase(std::find(remaining.begin(), remaining.end(), pick));
    }
    if (ok) *ok = true;
    return out;
}

AssetDecodeFn Registry::find_codec(const std::string& fourcc) const {
    for (const auto& c : codecs_) if (c.fourcc == fourcc) return c.fn;
    return nullptr;
}

std::size_t Registry::count(ExtKind kind) const {
    switch (kind) {
        case EXT_ECS_SYSTEM: return ecs_.size();
        case EXT_ASSET_CODEC: return codecs_.size();
        default: return named_[kind].size();
    }
}

static Registry* self(void* ctx) { return static_cast<Registry*>(ctx); }

static void thunk_ecs(void* ctx, const char* id, const char* const* after, int32_t n, SimSystemFn fn) {
    std::vector<std::string> deps;
    for (int32_t i = 0; i < n; ++i) deps.emplace_back(after[i]);
    self(ctx)->add_ecs_system(id, std::move(deps), fn);
}
static void thunk_codec(void* ctx, const char* fourcc, AssetDecodeFn fn) {
    self(ctx)->add_asset_codec(fourcc, fn);
}
static void thunk_render(void* ctx, const char* id, OpaqueFn fn) {
    self(ctx)->add_named(EXT_RENDER_PASS, id, "", reinterpret_cast<void*>(fn));
}
static void thunk_input(void* ctx, const char* id, OpaqueFn fn) {
    self(ctx)->add_named(EXT_INPUT_SOURCE, id, "", reinterpret_cast<void*>(fn));
}
static void thunk_audio(void* ctx, const char* id, OpaqueFn fn) {
    self(ctx)->add_named(EXT_AUDIO_BUS, id, "", reinterpret_cast<void*>(fn));
}
static void thunk_ui(void* ctx, const char* id, const char* title, UiDrawFn fn) {
    self(ctx)->add_named(EXT_UI_PANEL, id, title, reinterpret_cast<void*>(fn));
}
static void thunk_log(void*, const char* msg) {
    std::fprintf(stderr, "[plugin] %s\n", msg);
}

HostApi Registry::make_host_api() {
    HostApi api{};
    api.ctx = this;
    api.api_version = PLUGIN_API_VERSION;
    api.register_ecs_system = thunk_ecs;
    api.register_asset_codec = thunk_codec;
    api.register_render_pass = thunk_render;
    api.register_input_source = thunk_input;
    api.register_audio_bus = thunk_audio;
    api.register_ui_panel = thunk_ui;
    api.log = thunk_log;
    return api;
}
