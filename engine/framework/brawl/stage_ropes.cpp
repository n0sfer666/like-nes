#include "stage_ropes.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "body_kind.hpp"
#include "rope_ride.hpp"

namespace framework::brawl {

namespace {

static_assert(std::is_trivially_copyable_v<StageRopes>, "a rollback snapshot of the ropes must not allocate");

[[noreturn]] void refuse(const char* why) {
    std::fprintf(stderr, "[brawl] %s\n", why);
    std::abort();
}

} // namespace

StageRopes::StageRopes(const scene::LevelObjects& level) {
    if (level.ropes.size() > at.size()) refuse("a level holds more ropes than ROPES_MAX");
    count = static_cast<uint32_t>(level.ropes.size());
}

bool rides(const StageRopes* ropes, EntId id) {
    if (ropes == nullptr || id == EntId{}) return false;
    for (const RopeState& r : ropes->live())
        if (r.rider == id) return true;
    return false;
}

void step_ropes(StageRopes* ropes, BodyPool& pool, std::span<const Command> commands, StageSections& sections,
                const BrawlWorld& w) {
    const size_t count = w.level != nullptr ? w.level->ropes.size() : 0;
    if (ropes == nullptr) {
        if (count != 0) refuse("a level with ropes steps without ropes");
        return;
    }
    if (ropes->count != count) refuse("ropes were built for a level with another number of ropes");
    if (count == 0) return;
    if (w.platform == nullptr) refuse("ropes step without a platform world");
    std::array<EntId, POOL_CAPACITY> gone{};
    uint32_t left = 0;
    for (size_t i = 0; i < count; ++i) {
        const EntId id = ride_rope(ropes->at[i], w.level->ropes[i], pool, commands, sections, w);
        if (!(id == EntId{})) gone[left++] = id;
    }
    for (uint32_t i = 0; i < pool.count; ++i) {
        const EntId id = pool.bodies[i].id;
        if (role_of(pool.bodies[i], w.kinds) != Role::Fighter) continue;
        if (std::find(gone.begin(), gone.begin() + left, id) != gone.begin() + left) continue;
        grab_rope(*ropes, *w.level, pool.bodies[i], i < commands.size() ? commands[i].input : BrawlInput{}, sections,
                  *w.platform);
    }
}

} // namespace framework::brawl
