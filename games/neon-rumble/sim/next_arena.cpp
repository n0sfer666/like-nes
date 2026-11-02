#include "next_arena.hpp"

namespace framework::brawl {

const scene::Arena* next_arena(const scene::LevelObjects& level, int16_t cleared) {
    const scene::Arena* next = nullptr;
    for (const scene::Arena& a : level.arenas)
        if (a.id > cleared && a.id <= INT16_MAX && (next == nullptr || a.id < next->id)) next = &a;
    return next;
}

} // namespace framework::brawl
