#include "ai_brain.hpp"

namespace framework::ai {

bool add_brain(Brains& b, brawl::EntId body, uint8_t state) {
    if (body.seq == 0 || b.count >= BRAINS_MAX || find_brain(b, body) != nullptr) return false;
    uint32_t i = b.count;
    for (; i > 0 && b.at[i - 1].body.seq > body.seq; --i) b.at[i] = b.at[i - 1];
    b.at[i] = Brain{body, state, 0, {}};
    ++b.count;
    return true;
}

Brain* find_brain(Brains& b, brawl::EntId body) {
    for (uint32_t i = 0; i < b.count; ++i)
        if (b.at[i].body == body) return &b.at[i];
    return nullptr;
}

} // namespace framework::ai
