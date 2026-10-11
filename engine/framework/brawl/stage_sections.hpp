#pragma once
#include <array>
#include <cstdint>

#include "body_pool.hpp"
#include "character_state.hpp"

namespace framework::brawl {

struct SectionBody {
    EntId id;
    character::Character c;
};

// Тела в платформенной секции — запись на тело в порядке `seq`, как в пуле. Тела belt здесь нет:
// хеш пула, голдены belt и снапшот пула не двигаются оттого, что в уровне появилась секция.
struct StageSections {
    std::array<SectionBody, POOL_CAPACITY> at{};
    uint32_t count = 0;

    SectionBody* find(EntId id);
    const SectionBody* find(EntId id) const;
    bool add(EntId id, const character::Character& c);
    void drop(EntId id);
    void prune(const BodyPool& pool);
};

} // namespace framework::brawl
