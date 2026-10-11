#include "stage_sections.hpp"

namespace framework::brawl {

SectionBody* StageSections::find(EntId id) {
    for (uint32_t i = 0; i < count; ++i)
        if (at[i].id == id) return &at[i];
    return nullptr;
}

const SectionBody* StageSections::find(EntId id) const {
    for (uint32_t i = 0; i < count; ++i)
        if (at[i].id == id) return &at[i];
    return nullptr;
}

bool StageSections::add(EntId id, const character::Character& c) {
    if (count == at.size() || find(id) != nullptr) return false;
    uint32_t i = count++;
    for (; i > 0 && id.seq < at[i - 1].id.seq; --i) at[i] = at[i - 1];
    at[i] = SectionBody{id, c};
    return true;
}

void StageSections::drop(EntId id) {
    for (uint32_t i = 0; i < count; ++i) {
        if (!(at[i].id == id)) continue;
        for (uint32_t j = i + 1; j < count; ++j) at[j - 1] = at[j];
        at[--count] = SectionBody{};
        return;
    }
}

void StageSections::prune(const BodyPool& pool) {
    for (uint32_t i = count; i-- > 0;)
        if (pool.find(at[i].id) == nullptr) drop(at[i].id);
}

} // namespace framework::brawl
