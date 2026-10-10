#pragma once
#include <array>
#include <cstdint>

#include "ent_id.hpp"

namespace framework::ai {

constexpr uint32_t TOKENS_MAX = 4;

struct TokenHolder {
    brawl::EntId body;
    uint32_t taken = 0;
};

// Право бить на арене: держателей не больше capacity, остальные ждут. Ёмкость и таймаут — данные
// игры по сложности и числу игроков, но лежат в снапшоте: вход игрока посреди арены меняет ёмкость
// на тике, и переигранный тик обязан увидеть ту же.
struct AttackTokens {
    std::array<TokenHolder, TOKENS_MAX> held{};
    uint32_t count = 0;
    uint32_t capacity = 1;
    uint32_t timeout = 0;
};

bool holds_token(const AttackTokens& t, brawl::EntId body);
bool try_take_token(AttackTokens& t, brawl::EntId body, uint32_t tick);
// Освобождение не держащего — no-op: попадание и смерть в один тик освобождают один раз.
bool release_token(AttackTokens& t, brawl::EntId body);
// Таймаут считается от тика взятия; timeout = 0 — без таймаута.
void expire_tokens(AttackTokens& t, uint32_t tick);

} // namespace framework::ai
