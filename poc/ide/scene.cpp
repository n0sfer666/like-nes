#include "scene.hpp"

namespace ide {

void register_types(flecs::world& w) {
    w.component<fix32>()
        .opaque(flecs::I32)
        .serialize([](const flecs::serializer* s, const fix32* d) {
            return s->value(flecs::I32, &d->raw);
        })
        .assign_int([](fix32* d, int64_t v) { d->raw = static_cast<int32_t>(v); });

    w.component<std::string>()
        .opaque(flecs::String)
        .serialize([](const flecs::serializer* s, const std::string* d) {
            const char* str = d->c_str();
            return s->value(flecs::String, &str);
        })
        .assign_string([](std::string* d, const char* v) { *d = v; });

    w.component<Name>().member<std::string>("value");
    w.component<Parent>().member<uint64_t>("guid");
    w.component<Position>().member<fix32>("x").member<fix32>("y");
    w.component<Velocity>().member<fix32>("x").member<fix32>("y");
}

Scene::Scene() { register_types(world_); }

flecs::entity Scene::create(uint64_t guid) {
    auto it = by_guid_.find(guid);
    if (it != by_guid_.end()) it->second.destruct();
    flecs::entity e = world_.entity();
    by_guid_[guid] = e;
    return e;
}

flecs::entity Scene::get(uint64_t guid) const {
    auto it = by_guid_.find(guid);
    return it == by_guid_.end() ? flecs::entity() : it->second;
}

bool Scene::exists(uint64_t guid) const { return by_guid_.count(guid) != 0; }

void Scene::destroy(uint64_t guid) {
    auto it = by_guid_.find(guid);
    if (it != by_guid_.end()) {
        it->second.destruct();
        by_guid_.erase(it);
    }
}

void Scene::clear() {
    for (auto& [guid, e] : by_guid_) e.destruct();
    by_guid_.clear();
}

} // namespace ide
