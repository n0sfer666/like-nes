#include <chrono>
#include <cinttypes>
#include <cstdio>

#include "framework_ai_fixture.hpp"
#include "framework_brawl_check.hpp"
#include "session.hpp"

namespace {

using namespace framework::ai;
using framework::ai::test::AiBrawl;
using framework::brawl::test::check;
using framework::rollback::Session;
using framework::rollback::Tick;

constexpr Tick TICKS = 600;
constexpr Tick DELAY = 8;
constexpr Tick CHECK_EVERY = 25;
constexpr uint64_t GOLDEN = 0x11297ef67756ad54ull;

struct Run {
    uint64_t hash = 0, world = 0;
    int64_t first_bad = -1;
    uint32_t rollbacks = 0, replayed = 0;
    uint32_t live_rollbacks = 0, live_replayed = 0;
    uint64_t step_ns = 0;
};

uint64_t straight(uint64_t* per_tick, AiBrawl& sim) {
    framework::brawl::BrawlInput row[2];
    for (Tick t = 0; t < TICKS; ++t) {
        for (uint32_t p = 0; p < 2; ++p) row[p] = test::scripted(t, p);
        sim.step(row);
        per_tick[t] = sim.hash();
    }
    return sim.hash();
}

Run delayed(const uint64_t* per_tick, uint32_t depth, void (*forget)(AiState&, const AiState&),
          bool checkpoints = true) {
    AiBrawl sim;
    sim.forget = forget;
    Session<AiBrawl> ses;
    ses.reset(2, depth);
    Run r;
    const auto start = std::chrono::steady_clock::now();
    for (Tick t = 0; t < TICKS; ++t) {
        ses.deliver(t, 0, test::scripted(t, 0));
        if (t >= DELAY) ses.deliver(t - DELAY, 1, test::scripted(t - DELAY, 1));
        ses.advance(sim);
        if (!checkpoints || (t + 1) % CHECK_EVERY != 0) continue;
        for (Tick k = (t >= DELAY ? t - DELAY + 1 : 0); k <= t; ++k) ses.deliver(k, 1, test::scripted(k, 1));
        ses.settle(sim);
        if (sim.hash() != per_tick[t] && r.first_bad < 0) r.first_bad = static_cast<int64_t>(t);
    }
    r.live_rollbacks = ses.rollbacks();
    r.live_replayed = ses.replayed();
    for (Tick k = TICKS - DELAY; k < TICKS; ++k) ses.deliver(k, 1, test::scripted(k, 1));
    ses.settle(sim);
    const auto spent = std::chrono::steady_clock::now() - start;
    r.hash = sim.hash();
    r.world = sim.world_hash();
    r.rollbacks = ses.rollbacks();
    r.replayed = ses.replayed();
    const uint64_t steps = TICKS + r.replayed;
    r.step_ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(spent).count()) / steps;
    return r;
}

void test_resim_with_ai_matches_the_straight_run(const uint64_t* per_tick, uint64_t want) {
    const Run r = delayed(per_tick, DELAY, nullptr);
    if (r.first_bad >= 0) std::printf("  first divergent tick: %lld\n", static_cast<long long>(r.first_bad));
    check(r.first_bad < 0, "every checkpoint of the resimulated run matches the straight one");
    check(r.hash == want, "and so does the final state");
    // Без откатов гейт зелен вакуумно; откат на всю задержку — то, что спека называет ресимом 8 тиков.
    check(r.rollbacks >= 20, "control: the delay really forced rollbacks");
    std::printf("  resim: rollbacks=%u replayed=%u step=%llu ns\n", r.rollbacks, r.replayed,
                static_cast<unsigned long long>(r.step_ns));
    // Контрольные точки досылают ввод раньше срока и укорачивают откаты, хвостовой settle — тоже;
    // глубину судит прогон без них, до хвоста.
    const Run deep = delayed(per_tick, DELAY, nullptr, false);
    check(deep.hash == want, "the run without checkpoints matches too");
    check(deep.live_rollbacks >= 20 && deep.live_replayed == deep.live_rollbacks * DELAY,
          "each of its rollbacks replayed 8 ticks");
    std::printf("  resim 8: rollbacks=%u replayed=%u step=%llu ns\n", deep.rollbacks, deep.replayed,
                static_cast<unsigned long long>(deep.step_ns));
}

void test_without_rollback_the_run_diverges(const uint64_t* per_tick, uint64_t want) {
    const Run r = delayed(per_tick, 0, nullptr);
    check(r.first_bad >= 0 && r.hash != want, "control: depth 0 cannot rewind, and the run diverges");
}

// Ресим сверяет прогон сам с собой и слеп к расхождению -O0 с -O3 или ОС с ОС; это ловит голден
// исхода боя, который CI сверяет в Release на трёх ОС и в Debug.
void test_the_ai_really_fought(const AiBrawl& sim, uint64_t want) {
    std::printf("ai fight hash = 0x%016" PRIx64 "\n", want);
    check(want == GOLDEN, "golden of the AI fight");
    check(sim.taken >= 3, "control: enemies took attack tokens");
    check(sim.brains_dropped >= 1, "control: knocked-out enemies lost their brains");
    check(sim.tokens_orphaned >= 1, "control: a token held by a knocked-out body was released by the drop");
    check(sim.ai.rng.state != 0x5eed, "control: the AI drew from the world RNG");
    std::printf("  fight: tokens taken=%u knocked out=%u brains dropped=%u orphaned tokens=%u\n", sim.taken,
                sim.knocked_out, sim.brains_dropped, sim.tokens_orphaned);
}

struct Forgotten {
    const char* name;
    void (*forget)(AiState&, const AiState&);
};

const Forgotten FORGOTTEN[] = {
    {"world RNG outside the snapshot", [](AiState& a, const AiState& l) { a.rng = l.rng; }},
    {"brain state outside the snapshot",
     [](AiState& a, const AiState& l) { for (uint32_t i = 0; i < a.brains.count; ++i) a.brains.at[i].state = l.brains.at[i].state; }},
    {"brain timer outside the snapshot",
     [](AiState& a, const AiState& l) { for (uint32_t i = 0; i < a.brains.count; ++i) a.brains.at[i].timer = l.brains.at[i].timer; }},
    {"brain target outside the snapshot",
     [](AiState& a, const AiState& l) { for (uint32_t i = 0; i < a.brains.count; ++i) a.brains.at[i].target = l.brains.at[i].target; }},
    {"brains outside the snapshot", [](AiState& a, const AiState& l) { a.brains = l.brains; }},
    {"attack tokens outside the snapshot", [](AiState& a, const AiState& l) { a.tokens = l.tokens; }},
};

// Судит хеш мира без ai_hash: забытое поле обязано изменить сам бой, а не только свою строку хеша.
void test_a_forgotten_field_breaks_the_resim(const uint64_t* per_tick, uint64_t want_world) {
    for (const Forgotten& f : FORGOTTEN) {
        const Run r = delayed(per_tick, DELAY, f.forget);
        const bool diverged = r.world != want_world;
        std::printf("  %s: %s\n", f.name, diverged ? "diverges" : "MATCHES");
        check(diverged, f.name);
    }
}

} // namespace

int main() {
    uint64_t per_tick[TICKS];
    AiBrawl sim;
    const uint64_t want = straight(per_tick, sim);
    test_the_ai_really_fought(sim, want);
    test_resim_with_ai_matches_the_straight_run(per_tick, want);
    test_without_rollback_the_run_diverges(per_tick, want);
    test_a_forgotten_field_breaks_the_resim(per_tick, sim.world_hash());
    return framework::brawl::test::verdict("framework-ai-resim");
}
