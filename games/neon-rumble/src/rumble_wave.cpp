#include "rumble_wave.hpp"

#include "ai_step.hpp"
#include "hash_mix.hpp"
#include "rumble_brain.hpp"
#include "rumble_floor.hpp"
#include "stage_camera_hash.hpp"
#include "stage_sections_hash.hpp"
#include "stage_step.hpp"
#include "stage_waves_hash.hpp"
#include "viewport_fit.hpp"
#include "wave_step.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;
namespace ai = framework::ai;

uint32_t players(const br::Seats& seats) {
    uint32_t n = 0;
    for (const br::Seat& s : seats.at) n += s.present ? 1 : 0;
    return n;
}

} // namespace

void Wave::entry(std::span<br::Body, br::SEATS> at, const br::DepthFloor& floor) const {
    const fix32 half = framework::graphics::view_zone_half().x - fix32::from_int(ENTRY_MARGIN);
    const fix32 lo = framework::max_fix(camera.center.x - half, floor.band.x0);
    const fix32 hi = framework::min_fix(camera.center.x + half, floor.band.x1);
    for (br::Body& b : at) {
        const fix32 x = framework::clamp_fix(b.pos.x, lo, hi);
        if (on_floor(floor, x, b.pos.z)) b.pos.x = x;
    }
}

void Wave::step(br::BodyPool& pool, const br::Seats& seats, br::BrawlWorld world, std::span<br::Command> commands,
                br::HitEvents& events) {
    world.level = &level;
    ai.tokens.capacity = TOKENS_BY_PLAYERS[players(seats)];
    ai::step_ai(ai, pool, world, tick, foe_brain(), commands);
    ++tick;
    br::step_stage(pool, std::span<const br::Command>(commands.data(), pool.count), world, camera, seats, sections,
                   events);
    was_arena = camera.arena;
    was_wave = waves.wave;
    br::step_waves(waves, camera, pool, seats, level, br::WaveRules{WAVE_LULL}, facts);
    spawned_count = 0;
    for (uint32_t i = 0; i < facts.count; ++i) {
        const br::EntId id = pool.spawn(foes[facts.starts[i]]);
        if (id == br::EntId{}) {
            ++facts.dropped;
            continue;
        }
        if (!ai::add_brain(ai.brains, id, FOE_START)) {
            pool.despawn(id);
            ++facts.dropped;
            continue;
        }
        spawned[spawned_count++] = id;
    }
}

uint64_t Wave::hash() const {
    uint64_t h = ai::ai_hash(ai);
    framework::physics::mix(h, tick);
    framework::physics::mix_u64(h, br::camera_hash(camera));
    framework::physics::mix_u64(h, br::sections_hash(sections));
    framework::physics::mix_u64(h, br::waves_hash(waves));
    return h;
}

} // namespace rumble
