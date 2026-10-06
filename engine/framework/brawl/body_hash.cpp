#include "body_hash.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert(std::is_trivially_copyable_v<BodyPool>, "the snapshot is a plain copy of the pool");
static_assert([] {
    [[maybe_unused]] auto [seq] = EntId{};
    [[maybe_unused]] auto [x, z, y, vx, vz, vy] = DepthBody{};
    [[maybe_unused]] auto [id, pos, facing, team, owner, hp, crushed, age] = Body{};
    [[maybe_unused]] auto [bodies, count, next_seq] = BodyPool{};
    return true;
}(), "a snapshot field was added: mix it here and add its row to framework_brawl_snapshot_test");

void mix(uint64_t& h, fix32 v) { physics::mix(h, static_cast<uint32_t>(v.raw)); }

void mix_body(uint64_t& h, const Body& b) {
    physics::mix(h, b.id.seq);
    mix(h, b.pos.x);
    mix(h, b.pos.z);
    mix(h, b.pos.y);
    mix(h, b.pos.vx);
    mix(h, b.pos.vz);
    mix(h, b.pos.vy);
    physics::mix(h, static_cast<uint32_t>(b.facing));
    physics::mix(h, b.team);
    physics::mix(h, b.owner.seq);
    physics::mix(h, static_cast<uint32_t>(b.hp));
    physics::mix(h, b.crushed ? 1u : 0u);
    physics::mix(h, b.age);
}

} // namespace

uint64_t state_hash(const BodyPool& pool) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix(h, pool.count);
    physics::mix(h, pool.next_seq);
    for (uint32_t i = 0; i < pool.count; ++i) mix_body(h, pool.bodies[i]);
    return h;
}

} // namespace framework::brawl
