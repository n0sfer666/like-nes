#include <array>
#include <cstdio>
#include <vector>

#include "ai_step.hpp"
#include "framework_brawl_check.hpp"

namespace {

using namespace framework::ai;
using framework::brawl::Body;
using framework::brawl::BodyPool;
using framework::brawl::Command;
using framework::brawl::EntId;
using framework::brawl::test::check;

EntId id(uint32_t seq) { return EntId{seq}; }

// Двойное освобождение: попадание и смерть в один тик. Второй вызов не должен снять чужой жетон —
// реализация «освободить кого-нибудь» прошла бы проверку счётчика, но не эту.
void test_double_release_frees_one_token() {
    AttackTokens t;
    t.capacity = 2;
    check(try_take_token(t, id(1), 10) && try_take_token(t, id(2), 11), "two holders under capacity 2");
    check(release_token(t, id(1)), "the hit releases the holder");
    check(!release_token(t, id(1)), "the death in the same tick is a no-op");
    check(t.count == 1 && holds_token(t, id(2)), "the other holder keeps its token");
    check(try_take_token(t, id(3), 12) && !try_take_token(t, id(4), 12), "exactly one slot came free");
}

void test_capacity_and_repeat_take() {
    AttackTokens t;
    check(try_take_token(t, id(5), 0), "capacity 1 gives the first asker the token");
    check(try_take_token(t, id(5), 3) && t.held[0].taken == 0, "asking again keeps the tick it was taken on");
    check(!try_take_token(t, id(6), 3), "the second asker waits");
    check(!try_take_token(t, EntId{}, 3) && !release_token(t, EntId{}), "seq 0 is nobody");
    t.capacity = 100;
    for (uint32_t s = 7; s < 20; ++s) try_take_token(t, id(s), 4);
    check(t.count == TOKENS_MAX, "capacity is clamped by the holder array");
}

void test_timeout_counts_from_the_take() {
    AttackTokens t;
    t.capacity = 2;
    t.timeout = 90;
    try_take_token(t, id(1), 100);
    try_take_token(t, id(2), 150);
    expire_tokens(t, 189);
    check(t.count == 2, "89 ticks after the take the token holds");
    expire_tokens(t, 190);
    check(t.count == 1 && holds_token(t, id(2)), "90 ticks after the take it expires, the later one stays");
    t.timeout = 0;
    expire_tokens(t, 100000);
    check(t.count == 1, "timeout 0 never expires");
}

void test_brains_are_ordered_by_seq() {
    Brains b;
    check(add_brain(b, id(9), 0) && add_brain(b, id(3), 1) && add_brain(b, id(6), 2), "three brains added");
    check(b.at[0].body.seq == 3 && b.at[1].body.seq == 6 && b.at[2].body.seq == 9, "kept in seq order");
    check(!add_brain(b, id(6), 0) && !add_brain(b, EntId{}, 0), "a duplicate and seq 0 are refused");
    for (uint32_t s = 20; s < 40; ++s) add_brain(b, id(s), 0);
    check(b.count == BRAINS_MAX, "the brain array does not overflow");
}

std::vector<uint32_t> order;

void note(Mind& m, Command& out) {
    order.push_back(m.body.id.seq);
    out.strike = m.brain.timer;
}

void grab(Mind& m, Command&) { try_take_token(m.tokens, m.body.id, m.tick); }

void test_step_ai_walks_brains_in_seq_order() {
    BodyPool pool;
    const EntId a = pool.spawn(Body{}), b = pool.spawn(Body{}), c = pool.spawn(Body{});
    AiState ai;
    add_brain(ai.brains, c, 0);
    add_brain(ai.brains, b, 0);
    add_brain(ai.brains, a, 0);
    ai.brains.at[0].target = c;
    ai.brains.at[0].timer = 3;
    pool.despawn(c);
    std::vector<Command> cmd(pool.count);
    const std::array<Think, 1> states{note};
    order.clear();
    step_ai(ai, pool, 1, states, cmd);
    check(order == std::vector<uint32_t>{a.seq, b.seq}, "live brains think in seq order, not insertion order");
    check(ai.brains.count == 2 && find_brain(ai.brains, c) == nullptr, "the brain of a vanished body is dropped");
    check(ai.brains.at[0].target.seq == 0, "a target without a body is cleared");
    check(cmd[0].strike == 2, "the timer ran down before the state was called");
}

void test_step_ai_skips_what_it_cannot_run() {
    BodyPool pool;
    const EntId a = pool.spawn(Body{}), b = pool.spawn(Body{});
    AiState ai;
    add_brain(ai.brains, a, 5);
    add_brain(ai.brains, b, 0);
    try_take_token(ai.tokens, a, 0);
    std::vector<Command> cmd(1);
    const std::array<Think, 1> states{note};
    order.clear();
    step_ai(ai, pool, 1, states, cmd);
    check(order.empty(), "neither a state outside the table nor a slot past the commands runs");
    check(holds_token(ai.tokens, a) && cmd[0].strike == Command{}.strike, "the skipped brain keeps its token and command");
}

// Жетон мёртвого тела с большим seq достаётся живому с меньшим в том же шаге: снятие мозгов идёт
// отдельным проходом до первого состояния.
void test_a_dead_holder_hands_over_in_the_same_step() {
    BodyPool pool;
    const EntId a = pool.spawn(Body{}), b = pool.spawn(Body{});
    AiState ai;
    add_brain(ai.brains, a, 0);
    add_brain(ai.brains, b, 0);
    try_take_token(ai.tokens, b, 0);
    pool.despawn(b);
    std::vector<Command> cmd(pool.count);
    const std::array<Think, 1> states{grab};
    step_ai(ai, pool, 1, states, cmd);
    check(ai.tokens.count == 1 && holds_token(ai.tokens, a), "the token of a vanished body is released before anyone thinks");
}

void test_rng_is_pinned() {
    WorldRng r{0x5eed};
    const std::array<uint32_t, 4> got{next_u32(r), next_u32(r), next_u32(r), below(r, 10)};
    const std::array<uint32_t, 4> want{0x09f1fd9du, 0x55327416u, 0x5d5bca46u, 4u};
    for (uint32_t v : got) std::printf("  rng %08x\n", v);
    check(got == want, "the world RNG sequence is the pinned one on every OS");
    WorldRng z;
    for (int i = 0; i < 1000; ++i) check(below(z, 7) < 7, "below stays in range");
}

struct Field {
    const char* name;
    void (*bump)(AiState&);
};

const Field FIELDS[] = {
    {"brain body", [](AiState& a) { a.brains.at[0].body.seq += 10; }},
    {"brain state", [](AiState& a) { a.brains.at[0].state += 1; }},
    {"brain timer", [](AiState& a) { a.brains.at[0].timer += 1; }},
    {"brain target", [](AiState& a) { a.brains.at[0].target.seq += 1; }},
    {"holder body", [](AiState& a) { a.tokens.held[0].body.seq += 10; }},
    {"holder taken", [](AiState& a) { a.tokens.held[0].taken += 1; }},
    {"capacity", [](AiState& a) { a.tokens.capacity += 1; }},
    {"timeout", [](AiState& a) { a.tokens.timeout += 1; }},
    {"rng", [](AiState& a) { a.rng.state += 1; }},
};

void test_hash_covers_every_field() {
    AiState base;
    add_brain(base.brains, id(1), 1);
    add_brain(base.brains, id(2), 0);
    base.brains.at[0].target = id(7);
    try_take_token(base.tokens, id(1), 30);
    for (const Field& f : FIELDS) {
        AiState s = base;
        f.bump(s);
        if (ai_hash(s) == ai_hash(base)) std::printf("  FAIL: hash misses %s\n", f.name);
        check(ai_hash(s) != ai_hash(base), f.name);
    }
}

} // namespace

int main() {
    test_hash_covers_every_field();
    test_double_release_frees_one_token();
    test_capacity_and_repeat_take();
    test_timeout_counts_from_the_take();
    test_brains_are_ordered_by_seq();
    test_step_ai_walks_brains_in_seq_order();
    test_step_ai_skips_what_it_cannot_run();
    test_a_dead_holder_hands_over_in_the_same_step();
    test_rng_is_pinned();
    return framework::brawl::test::verdict("framework-ai");
}
