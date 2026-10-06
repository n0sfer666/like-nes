#include "body_pool.hpp"

namespace framework::brawl {

EntId BodyPool::spawn(const Body& init) {
    if (count == POOL_CAPACITY) return EntId{};
    Body& b = bodies[count++];
    b = init;
    b.id = EntId{next_seq++};
    return b.id;
}

bool BodyPool::despawn(EntId id) {
    for (uint32_t i = 0; i < count; ++i) {
        if (!(bodies[i].id == id)) continue;
        for (uint32_t j = i + 1; j < count; ++j) bodies[j - 1] = bodies[j];
        bodies[--count] = Body{};
        return true;
    }
    return false;
}

const Body* BodyPool::find(EntId id) const {
    for (uint32_t i = 0; i < count; ++i)
        if (bodies[i].id == id) return &bodies[i];
    return nullptr;
}

Body* BodyPool::find(EntId id) {
    for (uint32_t i = 0; i < count; ++i)
        if (bodies[i].id == id) return &bodies[i];
    return nullptr;
}

} // namespace framework::brawl
