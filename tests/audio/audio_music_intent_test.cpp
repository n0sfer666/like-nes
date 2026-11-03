#include "music_intent.hpp"

#include <cstdio>
#include <utility>
#include <vector>

#include "rollback_timing.hpp"
#include "session.hpp"

namespace {
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("audio-music-intent: FAIL %s\n", why); } }
void basic() {
    using audio::MusicIntent;
    constexpr auto SETTLE = framework::rollback::SETTLE_TICKS<8>;
    static_assert(SETTLE == 25);
    static_assert(framework::rollback::settle_ticks(0) == 17);
    static_assert(framework::rollback::settle_ticks(UINT32_MAX) == 0);
    check(framework::rollback::settle_ticks(UINT32_MAX - framework::rollback::LEAD - 1) == UINT32_MAX, "largest valid depth does not wrap");
    MusicIntent invalid(0);
    check(!invalid.valid() && !invalid.observe(0, 0) && !invalid.stable_track(), "invalid threshold is inert");
    MusicIntent one(1);
    check(one.observe(0, 0) == 0 && one.stable_track() == 0 && !one.observe(1, 0), "track0 is valid, unchanged stable track does not repeat");
    check(one.observe(2, UINT64_MAX) == UINT64_MAX, "full64bit track identity");
    for (uint32_t threshold : {2u, 17u, SETTLE}) {
        MusicIntent intent(threshold);
        for (uint32_t t = 0; t < threshold - 1; ++t) check(!intent.observe(t, 7), "no change at thresholdminus1");
        for (uint32_t t = 0; t < threshold - 1; ++t) {
            check(!intent.observe(t, 9) && !intent.stable_track(), "replayed other wish cannot reset or advance stability");
        }
        check(intent.observe(threshold - 1, 7) == 7, "exact threshold after duplicate/replay observations");
        for (uint32_t t = threshold; t < threshold * 2 - 1; ++t) check(!intent.observe(t, 8) && intent.stable_track() == 7, "outgoing stable selection retained while incoming settles");
        check(intent.observe(threshold * 2 - 1, 8) == 8, "second stable choice");
        check(!intent.observe(threshold * 2, 8), "same current track no event");
    }
    MusicIntent reset(3);
    check(!reset.observe(0, 7) && !reset.observe(1, 7) && !reset.observe(2, 8) && !reset.observe(3, 7) &&
        !reset.observe(4, 7) && reset.observe(5, 7) == 7, "different forward wish restarts candidate");
    MusicIntent gap(3);
    check(!gap.observe(0, 7) && !gap.observe(1, 7) && !gap.observe(99, 7) && !gap.observe(100, 7) && gap.observe(101, 7) == 7, "gap resets stability");
    MusicIntent wrap(3);
    check(!wrap.observe(UINT32_MAX - 1, 7) && !wrap.observe(UINT32_MAX, 7) && !wrap.observe(UINT32_MAX - 2, 8) &&
        wrap.observe(0, 7) == 7, "modular tick wrap and old prewrap replay");
    MusicIntent limit(UINT32_MAX);
    for (uint32_t t = 0; t < 100; ++t) check(!limit.observe(t, 7), "large valid threshold does not overflow early");
    MusicIntent restart(2);
    check(!restart.observe(400, 7) && restart.observe(401, 7) == 7, "first tick epoch");
    restart = MusicIntent(2);
    check(!restart.stable_track() && !restart.observe(0, 9) && restart.observe(1, 9) == 9, "new instance starts a fresh tick epoch");
}
struct Input {
    uint32_t track = 0;
    bool operator==(const Input&) const = default;
};
struct Snapshot { uint32_t tick = 0, track = 0; };
struct Sim {
    using Input = ::Input;
    using Snapshot = ::Snapshot;
    Snapshot state;
    audio::MusicIntent* music;
    std::vector<std::pair<uint32_t, uint64_t>> changes;
    void save(Snapshot& out) const { out = state; }
    void restore(const Snapshot& in) { state = in; }
    void step(const Input* input) {
        state.track = input[0].track;
        if (music) if (const auto selected = music->observe(state.tick, state.track)) changes.emplace_back(state.tick, *selected);
        ++state.tick;
    }
};
void rollback() {
    using namespace framework::rollback;
    constexpr uint32_t DEPTH = 8, WAIT = SETTLE_TICKS<DEPTH>;
    static_assert(Session<Sim>::LEAD == LEAD);
    Session<Sim> session; session.reset(1, DEPTH);
    audio::MusicIntent intent(WAIT); Sim sim{{}, &intent, {}};
    for (uint32_t t = 0; t < WAIT; ++t) { check(session.deliver(t, 0, {11}), "initial input"); session.advance(sim); }
    check(session.deliver(WAIT, 0, {22}), "candidate input");
    for (uint32_t i = 0; i < WAIT - 1; ++i) session.advance(sim);
    check(intent.stable_track() == 11 && session.tick() == WAIT * 2 - 1, "candidate remains unselected at exact thresholdminus1");
    const auto dirty = session.tick() - DEPTH;
    check(session.deliver(dirty, 0, {11}), "late input at maximum rollback depth");
    session.settle(sim);
    check(session.rollbacks() == 1 && session.replayed() == DEPTH && !session.conflicts() && !session.too_deep(), "actual Session replayed max depth, no conflicting confirmations");
    check(intent.stable_track() == 11 && sim.changes.size() == 1, "replay cannot select a transient track or inflate stability");
    check(session.deliver(session.tick(), 0, {22}), "fresh forward wish after replay"); session.advance(sim);
    check(sim.changes == std::vector<std::pair<uint32_t, uint64_t>>{{WAIT - 1, 11}, {WAIT * 2 - 1, 22}} &&
        intent.stable_track() == 22, "only unique forward observations produce exact transition ticks");
    Session<Sim> reference; reference.reset(1, DEPTH); Sim without{{}, nullptr, {}};
    for (uint32_t t = 0; t < session.tick(); ++t) {
        const uint32_t track = t < WAIT || (t >= dirty && t < WAIT * 2 - 1) ? 11 : 22;
        check(reference.deliver(t, 0, {track}), "reference input"); reference.advance(without);
    }
    check(without.state.tick == sim.state.tick && without.state.track == sim.state.track, "observer remains outside restored simulation state");
}
}
int main() {
    basic(); rollback();
    std::puts(fails ? "audio-music-intent: FAIL" : "audio-music-intent: PASS stable forward ticks, wrap, gaps and actual max-depth rollback");
    return fails ? 1 : 0;
}
