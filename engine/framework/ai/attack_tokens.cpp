#include "attack_tokens.hpp"

#include <algorithm>

namespace framework::ai {

namespace {

uint32_t index_of(const AttackTokens& t, brawl::EntId body) {
    for (uint32_t i = 0; i < t.count; ++i)
        if (t.held[i].body == body) return i;
    return t.count;
}

void erase_at(AttackTokens& t, uint32_t i) {
    for (uint32_t k = i + 1; k < t.count; ++k) t.held[k - 1] = t.held[k];
    --t.count;
    t.held[t.count] = TokenHolder{};
}

} // namespace

bool holds_token(const AttackTokens& t, brawl::EntId body) { return index_of(t, body) < t.count; }

bool try_take_token(AttackTokens& t, brawl::EntId body, uint32_t tick) {
    if (body.seq == 0) return false;
    if (holds_token(t, body)) return true;
    if (t.count >= std::min(t.capacity, TOKENS_MAX)) return false;
    t.held[t.count++] = TokenHolder{body, tick};
    return true;
}

bool release_token(AttackTokens& t, brawl::EntId body) {
    const uint32_t i = index_of(t, body);
    if (i >= t.count) return false;
    erase_at(t, i);
    return true;
}

void expire_tokens(AttackTokens& t, uint32_t tick) {
    if (t.timeout == 0) return;
    for (uint32_t i = t.count; i-- > 0;)
        if (tick - t.held[i].taken >= t.timeout) erase_at(t, i);
}

} // namespace framework::ai
