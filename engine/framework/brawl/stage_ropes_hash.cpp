#include "stage_ropes_hash.hpp"

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert([] {
    [[maybe_unused]] auto [arc, speed, rider, cooldown] = RopeState{};
    return true;
}(), "a rope field was added: mix it here and add its forget fixture to framework_brawl_rope_test");

} // namespace

uint64_t ropes_hash(const StageRopes& r) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix(h, r.count);
    for (const RopeState& s : r.live()) {
        physics::mix(h, static_cast<uint32_t>(s.arc.raw));
        physics::mix(h, static_cast<uint32_t>(s.speed.raw));
        physics::mix(h, s.rider.seq);
        physics::mix(h, static_cast<uint32_t>(s.cooldown));
    }
    return h;
}

} // namespace framework::brawl
