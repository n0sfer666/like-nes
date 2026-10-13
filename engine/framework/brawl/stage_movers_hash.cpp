#include "stage_movers_hash.hpp"

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert([] {
    [[maybe_unused]] auto [segment, direction, traveled] = PathFollow{};
    return true;
}(), "a follow field was added: mix it here and add its forget fixture to framework_brawl_movers_test");

} // namespace

uint64_t movers_hash(const StageMovers& m) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix_u64(h, m.world.hash());
    physics::mix(h, static_cast<uint32_t>(m.follow.size()));
    for (const PathFollow& f : m.follow) {
        physics::mix(h, f.segment);
        physics::mix(h, static_cast<uint32_t>(f.direction));
        physics::mix(h, static_cast<uint32_t>(f.traveled.raw));
    }
    return h;
}

} // namespace framework::brawl
