#include "stage_sections_hash.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert(std::is_trivially_copyable_v<StageSections>, "the snapshot is a plain copy of the sections");
static_assert([] {
    [[maybe_unused]] auto [id, c] = SectionBody{};
    [[maybe_unused]] auto [position, velocity, state, coyote_left, buffer_left, ladder_regrab_left, on_ground,
                           jump_active, jump_was_held, hit_ceiling, hit_wall, support, crushed] = character::Character{};
    return true;
}(), "a section field was added: mix it here and add its row to framework_brawl_section_test");

void mix_fix(uint64_t& h, fix32 v) { physics::mix(h, static_cast<uint32_t>(v.raw)); }

void mix_body(uint64_t& h, const SectionBody& s) {
    const character::Character& c = s.c;
    physics::mix(h, s.id.seq);
    mix_fix(h, c.position.x);
    mix_fix(h, c.position.y);
    mix_fix(h, c.velocity.x);
    mix_fix(h, c.velocity.y);
    physics::mix(h, static_cast<uint32_t>(c.state));
    physics::mix(h, c.coyote_left);
    physics::mix(h, c.buffer_left);
    physics::mix(h, c.ladder_regrab_left);
    physics::mix(h, c.on_ground ? 1u : 0u);
    physics::mix(h, c.jump_active ? 1u : 0u);
    physics::mix(h, c.jump_was_held ? 1u : 0u);
    physics::mix(h, c.hit_ceiling ? 1u : 0u);
    physics::mix(h, c.hit_wall ? 1u : 0u);
    physics::mix(h, c.support.index);
    physics::mix(h, c.crushed ? 1u : 0u);
}

} // namespace

uint64_t sections_hash(const StageSections& sections) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix(h, sections.count);
    for (uint32_t i = 0; i < sections.count; ++i) mix_body(h, sections.at[i]);
    return h;
}

} // namespace framework::brawl
