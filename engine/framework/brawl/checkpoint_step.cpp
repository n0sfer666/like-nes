#include "checkpoint_step.hpp"

#include <cstdint>

#include "depth_step.hpp"
#include "scene_objects.hpp"

namespace framework::brawl {

namespace {

bool reached(const BodyPool& pool, const Seats& seats, fix32 x) {
    for (const Seat& s : seats.at) {
        if (!s.present) continue;
        const Body* b = pool.find(s.body);
        if (b != nullptr && b->hp > 0 && grounded(b->pos) && !(b->pos.x < x)) return true;
    }
    return false;
}

} // namespace

bool step_checkpoints(LevelProgress& progress, const BodyPool& pool, const Seats& seats,
                      const scene::LevelObjects& level) {
    const int16_t was = progress.checkpoint;
    for (const scene::Checkpoint& c : level.checkpoints)
        if (c.order > progress.checkpoint && c.order <= INT16_MAX && reached(pool, seats, c.mark.at.x))
            progress.checkpoint = static_cast<int16_t>(c.order);
    return progress.checkpoint != was;
}

const scene::Checkpoint* find_checkpoint(const scene::LevelObjects& level, int16_t order) {
    if (order == NO_CHECKPOINT) return nullptr;
    for (const scene::Checkpoint& c : level.checkpoints)
        if (c.order == order) return &c;
    return nullptr;
}

} // namespace framework::brawl
