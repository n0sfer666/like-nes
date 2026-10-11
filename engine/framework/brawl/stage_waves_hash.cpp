#include "stage_waves_hash.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert(std::is_trivially_copyable_v<StageWaves>, "the snapshot is a plain copy of the waves");
static_assert([] {
    [[maybe_unused]] auto [cleared, wave, lull, seen] = StageWaves{};
    return true;
}(), "a waves field was added: mix it here and add its row to framework_brawl_wave_test");

} // namespace

uint64_t waves_hash(const StageWaves& waves) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix(h, static_cast<uint32_t>(static_cast<int32_t>(waves.cleared)));
    physics::mix(h, static_cast<uint32_t>(waves.wave));
    physics::mix(h, waves.lull);
    physics::mix(h, static_cast<uint32_t>(waves.seen.raw));
    return h;
}

} // namespace framework::brawl
