#pragma once
#include "components.hpp"
#include <flecs.h>
#include <cstdint>
#include <map>

// Scene — обёртка над flecs-миром + карта стабильных entity-GUID (источник истины на диске)
// ↔ нестабильных flecs-entity-id. std::map упорядочен по GUID → детерминированная итерация без
// доп. сортировки (гейт 1: round-trip байт-идентичен).
namespace ide {

class Scene {
public:
    Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    flecs::entity create(uint64_t guid);
    flecs::entity get(uint64_t guid) const;
    bool exists(uint64_t guid) const;
    void destroy(uint64_t guid);
    void clear();

    const std::map<uint64_t, flecs::entity>& entities() const { return by_guid_; }
    flecs::world& world() { return world_; }
    const flecs::world& world() const { return world_; }

private:
    flecs::world world_;
    std::map<uint64_t, flecs::entity> by_guid_;
};

} // namespace ide
