#include "progress_hash.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert(std::is_trivially_copyable_v<LevelProgress>, "the snapshot is a plain copy of the progress");
static_assert([] {
    [[maybe_unused]] auto [checkpoint] = LevelProgress{};
    return true;
}(), "a progress field was added: mix it here and add its row to framework_brawl_progress_test");

} // namespace

uint64_t progress_hash(const LevelProgress& progress) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix(h, static_cast<uint32_t>(static_cast<int32_t>(progress.checkpoint)));
    return h;
}

} // namespace framework::brawl
