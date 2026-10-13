#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "framework_ai_bench_fight.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_crossplay.hpp"
#include "framework_brawl_scenarios.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "session.hpp"

namespace {

using namespace framework::ai::bench;
using framework::brawl::SEATS;
using framework::brawl::test::check;
using framework::rollback::Session;
using framework::rollback::Tick;
namespace scenario = framework::brawl::scenario;

constexpr Tick FRAMES = 600;
constexpr Tick DELAY = 8;
constexpr uint64_t FRAME_NS = 16666667;
constexpr uint64_t GOLDEN = 0x0f60b3fbe989d27eull;

constexpr uint32_t SCENARIO_COUNT = sizeof(scenario::SCENARIOS) / sizeof(scenario::SCENARIOS[0]);
static_assert(SCENARIO_COUNT == framework::brawl::CROSSPLAY_SCENARIOS, "the bench prints every crossplay scenario");

struct Measure {
    std::vector<uint64_t> frame_ns;
    uint64_t hash = 0;
    uint32_t rollbacks = 0, replayed = 0, too_deep = 0;
};

void straight(Fight& sim) {
    BrawlInput row[SEATS];
    for (Tick t = 0; t < DELAY + FRAMES; ++t) {
        for (uint32_t p = 0; p < SEATS; ++p) row[p] = scripted(t, p);
        sim.step(row);
    }
}

// Ввод P2 приезжает на DELAY тиков позже: каждый кадр после разгона — откат на всю глубину,
// переигрывание и новый тик. Время кадра — ровно `advance`, как его заплатит игра.
Measure resimulated() {
    Fight sim;
    Session<Fight> ses;
    ses.reset(SEATS, DELAY);
    Measure m;
    m.frame_ns.reserve(FRAMES);
    for (Tick t = 0; t < DELAY + FRAMES; ++t) {
        ses.deliver(t, 0, scripted(t, 0));
        if (t >= DELAY) ses.deliver(t - DELAY, 1, scripted(t - DELAY, 1));
        const auto start = std::chrono::steady_clock::now();
        ses.advance(sim);
        const auto spent = std::chrono::steady_clock::now() - start;
        if (t >= DELAY)
            m.frame_ns.push_back(
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(spent).count()));
    }
    m.rollbacks = ses.rollbacks();
    m.replayed = ses.replayed();
    m.too_deep = ses.too_deep();
    for (Tick k = FRAMES; k < DELAY + FRAMES; ++k) ses.deliver(k, 1, scripted(k, 1));
    ses.settle(sim);
    m.hash = sim.hash();
    return m;
}

void print_frame(const char* name, uint64_t ns) {
    const uint64_t share = ns * 10000 / FRAME_NS;
    std::printf("  frame %s = %" PRIu64 ".%" PRIu64 " us, %" PRIu64 ".%02" PRIu64 " %% of a 16.6 ms frame\n", name,
                ns / 1000, ns % 1000 / 100, share / 100, share % 100);
}

void report_timing(Measure m) {
    std::sort(m.frame_ns.begin(), m.frame_ns.end());
    print_frame("p50", m.frame_ns[m.frame_ns.size() / 2]);
    print_frame("p99", m.frame_ns[m.frame_ns.size() * 99 / 100]);
    std::printf("  resim: rollbacks=%u replayed=%u too_deep=%u\n", m.rollbacks, m.replayed, m.too_deep);
}

void judge_fight(const Fight& sim, const Measure& m) {
    const Census& c = sim.least;
    std::printf("  fewest in a tick: players=%u enemies=%u props=%u shots=%u\n", c.players, c.enemies, c.props,
                c.shots);
    std::printf("  hits=%u peak=%u of %u in a tick, hits_dropped=%u, crates broken=%u, shots fired=%u\n", sim.hits,
                sim.hits_peak, framework::brawl::MAX_HIT_EVENTS, sim.hits_dropped, sim.crates_broken, sim.shots_fired);
    std::printf("resim bench fight hash = 0x%016" PRIx64 "\n", sim.hash());
    check(sim.hash() == GOLDEN, "golden of the bench fight");
    check(m.hash == sim.hash(), "the resimulated fight ends where the straight one does");
    check(m.rollbacks == FRAMES && m.replayed == FRAMES * DELAY && m.too_deep == 0,
          "every measured frame rolled back 8 ticks");
    check(c.players == SEATS && c.enemies == ENEMIES && c.props == PROPS && c.shots == SHOTS,
          "every tick ran the full umbrella budget");
    check(sim.hits_dropped == 0, "no hit event was dropped at the umbrella budget");
    check(sim.hits > 0 && sim.crates_broken > 0 && sim.shots_fired > SHOTS, "control: fists and shots landed");
}

std::string crossplay_text() {
    std::string text;
    char line[96];
    for (const scenario::Scenario& s : scenario::SCENARIOS) {
        std::snprintf(line, sizeof(line), "%s 0x%016" PRIx64 "\n", s.name, scenario::run(s));
        text += line;
    }
    return text;
}

void judge_crossplay(const std::string& against) {
    const std::string mine = crossplay_text();
    std::printf("crossplay scenarios:\n");
    for (std::size_t at = 0, end; at < mine.size(); at = end + 1) {
        end = mine.find('\n', at);
        std::printf("  %s\n", mine.substr(at, end - at).c_str());
    }
    if (against.empty()) return;
    std::string theirs;
    check(platform::read_text(against, theirs), "the crossplay file is read");
    const bool same = theirs == mine;
    std::printf("  against %s: %s\n", against.c_str(), same ? "match" : "DIFFER");
    if (!same) std::printf("%s", theirs.c_str());
    check(same, "the scenario hashes match the crossplay file");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string against;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--against") != 0) continue;
        if (i + 1 == argc) {
            std::fprintf(stderr, "brawl-resim-bench: --against needs a crossplay file\n");
            return 2;
        }
        against = argv[++i];
    }

    std::printf("brawl resim bench: %u players, %u enemies with AI, %u props + %u shots; resim %u ticks x %u frames\n",
                SEATS, ENEMIES, PROPS, SHOTS, DELAY, FRAMES);
    std::printf("  snapshot: %zu bytes\n", sizeof(Fight::Snapshot));
    Fight sim;
    straight(sim);
    const Measure m = resimulated();
    report_timing(m);
    judge_fight(sim, m);
    judge_crossplay(against);
    return framework::brawl::test::verdict("brawl-resim-bench");
}
