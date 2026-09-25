#include "arena.hpp"
#include "gpu.hpp"

#include <cstdio>

// Аудит #21 A·3·13: пул арены рос на каждый новый дескриптор без потолка. На живом устройстве, потому
// что предмет — именно создание текстур: MAX_SLOTS уникальных дескрипторов получают вид, следующий —
// отказ без аллокации, а уже созданный дескриптор у потолка по-прежнему переиспользуется.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("[arena-cap] FAIL: %s\n", what);
        ++fails;
    }
}

TargetDesc nth(uint32_t i) {
    return TargetDesc{8 + i, 8, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_RenderAttachment};
}

} // namespace

int main() {
    GpuContext gpu;
    if (!gpu.init(nullptr)) {
        std::printf("arena-cap: FAIL (no adapter)\n");
        gpu.shutdown();
        return 1;
    }
    TargetArena arena;
    arena.init(gpu.device);
    arena.begin_frame();

    bool all = true;
    for (uint32_t i = 0; i < TargetArena::MAX_SLOTS; ++i) all = arena.acquire(nth(i)) != nullptr && all;
    check(all, "every descriptor under the cap gets a view");
    check(arena.allocations() == TargetArena::MAX_SLOTS, "one texture per descriptor under the cap");

    const uint32_t over = static_cast<uint32_t>(TargetArena::MAX_SLOTS);
    check(arena.acquire(nth(over)) == nullptr, "descriptor over the cap is refused");
    check(arena.acquire(nth(over + 1)) == nullptr, "refusal persists");
    check(arena.pool_size() == TargetArena::MAX_SLOTS, "pool does not grow over the cap");
    check(arena.allocations() == TargetArena::MAX_SLOTS, "refusal allocates nothing");
    check(arena.refusals() == 2, "every refusal is counted");

    arena.begin_frame();
    check(arena.acquire(nth(0)) != nullptr, "existing descriptor is reused at the cap");
    check(arena.allocations() == TargetArena::MAX_SLOTS, "reuse at the cap allocates nothing");

    std::printf("arena-cap: %s - %zu slot(s), %llu refusal(s)\n", fails == 0 ? "PASS" : "FAIL",
                arena.pool_size(), static_cast<unsigned long long>(arena.refusals()));
    arena.shutdown();
    gpu.shutdown();
    return fails == 0 ? 0 : 1;
}
