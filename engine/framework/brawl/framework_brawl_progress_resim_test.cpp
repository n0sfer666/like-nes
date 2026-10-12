#include <cinttypes>
#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_progress_fixture.hpp"
#include "session.hpp"

namespace {

using namespace framework::brawl;
using framework::rollback::Session;
using framework::rollback::Tick;
using test::check;
using test::Course;

constexpr Tick TICKS = 360;
constexpr Tick DELAY = 8;
constexpr Tick CHECK_EVERY = 25;
constexpr uint64_t GOLDEN = 0x9226b3f907514dc1ull;

struct Run {
    uint64_t hash = 0, world = 0;
    int64_t first_bad = -1;
    uint32_t rollbacks = 0, at_one = 0;
    int16_t peak = NO_CHECKPOINT;
};

void straight(uint64_t* per_tick, Course& sim) {
    BrawlInput row[SEATS];
    for (Tick t = 0; t < TICKS; ++t) {
        for (uint32_t p = 0; p < SEATS; ++p) row[p] = test::course(t, p);
        sim.step(row);
        per_tick[t] = sim.hash();
    }
}

Run delayed(const uint64_t* per_tick, bool forget) {
    Course sim;
    sim.forget = forget;
    Session<Course> ses;
    ses.reset(SEATS, DELAY);
    Run r;
    for (Tick t = 0; t < TICKS; ++t) {
        ses.deliver(t, 0, test::course(t, 0));
        if (t >= DELAY) ses.deliver(t - DELAY, 1, test::course(t - DELAY, 1));
        ses.advance(sim);
        if ((t + 1) % CHECK_EVERY != 0) continue;
        for (Tick k = (t >= DELAY ? t - DELAY + 1 : 0); k <= t; ++k) ses.deliver(k, 1, test::course(k, 1));
        ses.settle(sim);
        if (sim.hash() != per_tick[t] && r.first_bad < 0) r.first_bad = static_cast<int64_t>(t);
    }
    for (Tick k = TICKS - DELAY; k < TICKS; ++k) ses.deliver(k, 1, test::course(k, 1));
    ses.settle(sim);
    r.hash = sim.hash();
    r.world = sim.world_hash();
    r.rollbacks = ses.rollbacks();
    r.peak = sim.peak;
    r.at_one = sim.respawned_at_one;
    return r;
}

void test_the_course_really_ran(const Course& sim) {
    const Body* p2 = sim.pool.find(sim.seats.at[1].body);
    check(sim.progress.checkpoint == 0 && sim.peak == 0, "control: the straight run takes checkpoint 0 only");
    check(p2 != nullptr && p2->pos.x < fix32::from_int(300) && fix32::from_int(300 - 2 * DELAY) < p2->pos.x,
          "control: P2 stops short of checkpoint 1, closer than the delay can carry him");
    check(sim.died >= 3 && sim.respawned_at_one == 0, "control: P1 falls into the pit and stands up at checkpoint 0");
    std::printf("  course: deaths=%u progress=%d\n", sim.died, sim.progress.checkpoint);
}

void test_resim_matches_the_straight_run(const uint64_t* per_tick, uint64_t want) {
    const Run r = delayed(per_tick, false);
    check(r.rollbacks > 0, "control: the delayed input rolled back");
    check(r.peak == 1, "control: the mispredicted run took checkpoint 1");
    check(r.first_bad < 0, "every checkpoint of the delayed run matches the straight one");
    check(r.hash == want, "the delayed run ends where the straight one does");
}

// Судит хеш мира без progress_hash: забытый прогресс обязан сдвинуть сам респаун, а не только свою
// строку хеша.
void test_a_forgotten_progress_breaks_the_resim(uint64_t want_world) {
    uint64_t unused[TICKS] = {};
    const Run r = delayed(unused, true);
    check(r.at_one > 0, "control: the forgotten progress respawned P1 at checkpoint 1");
    const bool diverged = r.world != want_world;
    std::printf("  progress outside the snapshot: %s\n", diverged ? "diverges" : "MATCHES");
    check(diverged, "progress outside the snapshot");
}

} // namespace

int main() {
    static uint64_t per_tick[TICKS];
    Course sim;
    straight(per_tick, sim);
    std::printf("  progress course hash = 0x%016" PRIx64 "\n", sim.hash());
    check(sim.hash() == GOLDEN, "golden of the progress course");
    test_the_course_really_ran(sim);
    test_resim_matches_the_straight_run(per_tick, sim.hash());
    test_a_forgotten_progress_breaks_the_resim(sim.world_hash());
    return test::verdict("framework-brawl-progress-resim");
}
