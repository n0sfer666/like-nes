#include "body_chain.hpp"

#include "body_clip.hpp"

namespace framework::brawl {

namespace {

bool next_step(const Body& b, const Archetype& a, uint8_t& next) {
    if (!chain_root(a, b.queued.strike) || b.chain + 1u >= a.chain_count || b.clip != a.chain[b.chain]) return false;
    if (b.elapsed < a.cancel[b.chain] || b.struck.count == 0) return false;
    next = static_cast<uint8_t>(b.chain + 1u);
    return true;
}

void begin(Body& b, uint16_t clip, uint8_t step) {
    start_strike(b, clip);
    b.chain = step;
    b.queued = StrikeQueue{};
}

} // namespace

bool chain_root(const Archetype& a, uint16_t clip) { return a.chain_count > 0 && a.chain[0] == clip; }

void queue_strike(Body& b, uint16_t strike, const Archetype& a) {
    if (strike != NO_STRIKE && is_move(a, strike)) b.queued = StrikeQueue{strike, a.buffer_ticks};
}

bool take_strike(Body& b, const Archetype& a) {
    if (b.queued.ticks == 0) return false;
    uint8_t next = 0;
    if (next_step(b, a, next)) {
        begin(b, a.chain[next], next);
        return true;
    }
    if (can_strike(b, a)) {
        begin(b, b.queued.strike, chain_root(a, b.queued.strike) ? 0 : NO_CHAIN);
        return true;
    }
    if (--b.queued.ticks == 0) b.queued = StrikeQueue{};
    return false;
}

} // namespace framework::brawl
