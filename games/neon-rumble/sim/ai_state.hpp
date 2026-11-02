#pragma once
#include <cstdint>

#include "ai_brain.hpp"
#include "attack_tokens.hpp"
#include "world_rng.hpp"

namespace framework::ai {

using core::below;
using core::next_u32;
using core::WorldRng;

// Снапшот ИИ — простая копия этой структуры, хеш — ai_hash.
struct AiState {
    Brains brains;
    AttackTokens tokens;
    WorldRng rng;
};

uint64_t ai_hash(const AiState& s);

} // namespace framework::ai
