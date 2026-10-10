#include "ai_state.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::ai {

namespace {

static_assert(std::is_trivially_copyable_v<AiState>, "the snapshot is a plain copy of the AI state");
static_assert([] {
    [[maybe_unused]] auto [body, state, timer, target] = Brain{};
    [[maybe_unused]] auto [at, count] = Brains{};
    [[maybe_unused]] auto [holder, taken] = TokenHolder{};
    [[maybe_unused]] auto [held, held_count, capacity, timeout] = AttackTokens{};
    [[maybe_unused]] auto [rng_state] = WorldRng{};
    [[maybe_unused]] auto [brains, tokens, rng] = AiState{};
    return true;
}(), "an AI snapshot field was added: mix it here and add its row to framework_ai_test");

} // namespace

uint64_t ai_hash(const AiState& s) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix(h, s.brains.count);
    for (uint32_t i = 0; i < s.brains.count; ++i) {
        const Brain& b = s.brains.at[i];
        physics::mix(h, b.body.seq);
        physics::mix(h, b.state);
        physics::mix(h, b.timer);
        physics::mix(h, b.target.seq);
    }
    physics::mix(h, s.tokens.count);
    physics::mix(h, s.tokens.capacity);
    physics::mix(h, s.tokens.timeout);
    for (uint32_t i = 0; i < s.tokens.count; ++i) {
        physics::mix(h, s.tokens.held[i].body.seq);
        physics::mix(h, s.tokens.held[i].taken);
    }
    physics::mix_u64(h, s.rng.state);
    return h;
}

} // namespace framework::ai
