#include "ai_target.hpp"

#include <climits>

namespace framework::ai {

int64_t axis_gap(fix32 a, fix32 b) {
    const int64_t d = int64_t{a.raw} - b.raw;
    return d < 0 ? -d : d;
}

const brawl::Body* nearest_body(const brawl::BodyPool& pool, const brawl::Body& from, uint8_t team) {
    const brawl::Body* best = nullptr;
    int64_t best_d = INT64_MAX;
    for (uint32_t i = 0; i < pool.count; ++i) {
        const brawl::Body& b = pool.bodies[i];
        if (b.team != team || b.hp <= 0) continue;
        const int64_t d = axis_gap(b.pos.x, from.pos.x) + axis_gap(b.pos.z, from.pos.z);
        if (d > best_d || (d == best_d && best != nullptr && b.kind >= best->kind)) continue;
        best_d = d;
        best = &b;
    }
    return best;
}

} // namespace framework::ai
