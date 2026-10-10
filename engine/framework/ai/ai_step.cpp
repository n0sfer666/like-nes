#include "ai_step.hpp"

namespace framework::ai {

namespace {

void drop_brain(AiState& ai, uint32_t i) {
    release_token(ai.tokens, ai.brains.at[i].body);
    for (uint32_t k = i + 1; k < ai.brains.count; ++k) ai.brains.at[k - 1] = ai.brains.at[k];
    --ai.brains.count;
    ai.brains.at[ai.brains.count] = Brain{};
}

} // namespace

void step_ai(AiState& ai, const brawl::BodyPool& pool, const brawl::BrawlWorld& world, uint32_t tick,
             std::span<const Think> states, std::span<brawl::Command> commands) {
    expire_tokens(ai.tokens, tick);
    for (uint32_t i = ai.brains.count; i-- > 0;)
        if (pool.find(ai.brains.at[i].body) == nullptr) drop_brain(ai, i);
    for (uint32_t i = 0; i < ai.brains.count; ++i) {
        Brain& b = ai.brains.at[i];
        const brawl::Body* body = pool.find(b.body);
        if (pool.find(b.target) == nullptr) b.target = brawl::EntId{};
        if (b.timer > 0) --b.timer;
        const auto slot = static_cast<size_t>(body - pool.bodies.data());
        if (b.state >= states.size() || slot >= commands.size()) continue;
        Mind mind{b, *body, pool, world, ai.tokens, ai.rng, tick};
        states[b.state](mind, commands[slot]);
    }
}

} // namespace framework::ai
