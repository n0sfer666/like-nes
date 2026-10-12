#include <cstdio>

#include "checkpoint_step.hpp"
#include "death_step.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_stage_fixture.hpp"
#include "progress_hash.hpp"

namespace {

using namespace framework::brawl;
using framework::scene::Checkpoint;
using test::at;
using test::check;
using test::Stage;

Checkpoint point(int32_t x, int32_t order) { return Checkpoint{{nullptr, {at(x), at(240)}}, order}; }

struct Course {
    Stage s;
    LevelProgress progress;

    explicit Course(int32_t x) {
        s.level.checkpoints = {point(300, 1), point(500, 2), point(400, 0)};
        s.seat(0, x);
    }

    bool step() { return step_checkpoints(progress, s.pool, s.seats, s.level); }
    Body& p1() { return s.player(0); }
};

int16_t taken_at(fix32 x) {
    Course c(0);
    c.p1().pos.x = x;
    c.step();
    return c.progress.checkpoint;
}

void edges() {
    check(taken_at(fix32::from_raw(at(300).raw - 1)) == NO_CHECKPOINT, "a raw short of the point takes nothing");
    check(taken_at(at(300)) == 1, "x on the point takes it");
    check(taken_at(at(399)) == 1 && taken_at(at(400)) == 1, "a smaller order past the point is not taken");
    check(taken_at(at(500)) == 2, "of several reached in one tick the largest order is taken");
}

void who() {
    Course air(320);
    air.p1().pos.y = at(4);
    check(!air.step() && air.progress.checkpoint == NO_CHECKPOINT, "an airborne player takes nothing");
    air.p1().pos.y = fix32{};
    check(air.step() && air.progress.checkpoint == 1, "control: the landed player takes the point he jumped over");
    Course rising(320);
    rising.p1().pos.vy = at(1);
    check(!rising.step(), "a player leaving the ground takes nothing");
    Course dead(320);
    dead.p1().hp = 0;
    check(!dead.step(), "a knocked-out player takes nothing");
    Course below(320);
    below.p1().hp = -1;
    check(!below.step(), "a player below hp 0 takes nothing");
    Course gone(320);
    gone.s.seats.at[0].present = false;
    check(!gone.step(), "an absent seat takes nothing");
    Course foe(0);
    foe.s.pool.spawn(test::dummy(600, 240, -1, 1, foe.s.arena.kinds[0]));
    check(!foe.step(), "a body without a seat takes nothing");
    Course two(0);
    two.s.seat(1, 320);
    check(two.step() && two.progress.checkpoint == 1, "the second seat takes the point too");
}

void forward_only() {
    Course c(500);
    check(c.step() && c.progress.checkpoint == 2, "the far point is taken");
    c.p1().pos.x = at(300);
    check(!c.step() && c.progress.checkpoint == 2, "walking back keeps the larger order");
    check(!c.step(), "a held point is not taken twice");
    Course odd(1000);
    odd.s.level.checkpoints = {point(10, -3), point(30, 32767), point(20, 32768)};
    check(odd.step() && odd.progress.checkpoint == 32767, "orders outside [0, 32767] are not taken");
    Course zero(1000);
    zero.s.level.checkpoints = {point(10, 0)};
    check(zero.step() && zero.progress.checkpoint == 0, "order 0 is taken");
}

void lookup() {
    framework::scene::LevelObjects l;
    l.checkpoints = {point(10, -1), point(20, 4), point(30, 4)};
    check(find_checkpoint(l, NO_CHECKPOINT) == nullptr, "no checkpoint finds nothing, even an order -1 in the level");
    check(find_checkpoint(l, 4) == &l.checkpoints[1], "the first point with the order is the respawn");
    check(find_checkpoint(l, 5) == nullptr, "a missing order finds nothing");
}

void deaths() {
    Stage s;
    Body& hit = s.seat(0, 100);
    Body& lying = s.seat(1, 200);
    lying.hp = -1;
    const EntId gone = s.pool.spawn(test::dummy(300, 240, 1, 1, s.arena.kinds[0]));
    const EntId hp_kept = s.pool.spawn(test::dummy(400, 240, 1, 1, s.arena.kinds[0]));
    Living living;
    note_living(s.pool, living);
    note_living(s.pool, living);
    check(living.count == 3, "living holds the bodies with hp > 0, once per call");
    hit.hp = -1;
    s.pool.find(hp_kept)->hp = 1;
    s.pool.despawn(gone);
    Body born = test::dummy(500, 240, 1, 1, s.arena.kinds[0]);
    born.hp = 0;
    s.pool.spawn(born);
    DeathFacts facts;
    facts.count = 9;
    step_deaths(s.pool, living, facts);
    check(facts.count == 1 && facts.at[0].body == s.seats.at[0].body,
          "only hp > 0 -> <= 0 dies: not one lying below 0, despawned, born at 0 or still standing");
    Living again;
    note_living(s.pool, again);
    step_deaths(s.pool, again, facts);
    check(facts.count == 0, "a body lying below hp 0 does not die again");
}

void fields() {
    LevelProgress one{1};
    LevelProgress two{2};
    if (progress_hash(one) == progress_hash(two)) {
        std::printf("  FAIL: progress field checkpoint is outside the hash\n");
        ++test::fails;
    }
}

} // namespace

int main() {
    edges();
    who();
    forward_only();
    lookup();
    deaths();
    fields();
    return test::verdict("framework-brawl-progress");
}
