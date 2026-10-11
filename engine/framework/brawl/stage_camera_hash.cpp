#include "stage_camera_hash.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert(std::is_trivially_copyable_v<StageCamera>, "the snapshot is a plain copy of the camera");
static_assert([] {
    [[maybe_unused]] auto [center, arena, scroll, ride, riding, settle] = StageCamera{};
    return true;
}(), "a camera field was added: mix it here and add its row to framework_brawl_camera_test");

} // namespace

uint64_t camera_hash(const StageCamera& camera) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix(h, static_cast<uint32_t>(camera.center.x.raw));
    physics::mix(h, static_cast<uint32_t>(camera.center.y.raw));
    physics::mix(h, static_cast<uint32_t>(static_cast<int32_t>(camera.arena)));
    physics::mix(h, static_cast<uint32_t>(camera.scroll.raw));
    physics::mix(h, camera.ride);
    physics::mix(h, camera.riding ? 1u : 0u);
    physics::mix(h, static_cast<uint32_t>(camera.settle.x.raw));
    physics::mix(h, static_cast<uint32_t>(camera.settle.y.raw));
    return h;
}

} // namespace framework::brawl
