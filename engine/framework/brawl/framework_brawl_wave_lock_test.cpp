#include "framework_brawl_check.hpp"
#include "framework_brawl_wave_fixture.hpp"

namespace {

using namespace framework::brawl;
using framework::scene::LevelObjects;
using test::area;
using test::at;
using test::check;
using test::Starts;
using test::Waves;

fix32 half_past(int32_t v) { return fix32::from_raw(at(v).raw + at(1).raw / 2); }

bool locks(int32_t x, int16_t cleared = NO_ARENA) {
    Waves w(x);
    w.waves.cleared = cleared;
    w.step();
    return w.facts.locked && w.s.camera.arena != NO_ARENA;
}

bool crosses(fix32 from, fix32 to, int16_t cleared = NO_ARENA) {
    Waves w(0);
    w.waves.cleared = cleared;
    w.waves.seen = from;
    w.s.camera.center.x = to;
    w.step();
    return w.facts.locked;
}

void edges() {
    check(!locks(591) && !locks(809), "the arena locks with the view across its edge");
    check(locks(592) && locks(808), "the arena does not lock with the view on its edge");
    check(!locks(1299, 0) && locks(1300, 0) && !locks(1301, 0), "a narrow arena locks off its middle");
}

void crossing() {
    check(crosses(half_past(1299), half_past(1300), 0) && crosses(half_past(1300), half_past(1299), 0),
          "a camera stepping over a narrow arena's middle misses the lock");
    check(!crosses(at(1290), half_past(1299), 0) && crosses(at(1290), at(1300), 0),
          "the lock fires short of the target or misses a step landing on it");
    check(crosses(at(500), at(592)) && !crosses(at(500), at(591)), "a step onto a wide arena's edge misses the lock");
    Waves w(1200);
    w.s.camera.center.x = at(1400);
    w.step();
    w.waves.cleared = 0;
    w.s.camera.center.x = at(1410);
    w.step();
    check(!w.facts.locked && w.waves.seen == at(1410), "the lock judges the path from a stale camera");
}

void order() {
    check(!locks(1300), "an arena locks before the one with a lower id is cleared");
    check(!locks(700, 0), "a cleared arena locks again");
    LevelObjects wide;
    wide.arenas.push_back({area(400, 0, 1000, 216), 40000});
    Waves far(700, wide);
    far.step();
    check(!far.facts.locked && far.s.camera.arena == NO_ARENA, "an arena id past int16 locks");
}

void first() {
    Waves w(700);
    w.step();
    check(w.s.camera.arena == 0 && w.waves.wave == 0 && w.starts() == Starts{2},
          "the first wave does not start in the lock tick");
    LevelObjects bare;
    bare.arenas.push_back({area(400, 0, 1000, 216), 0});
    Waves empty(700, bare);
    empty.step();
    check(empty.facts.locked && empty.facts.released && empty.s.camera.arena == NO_ARENA && empty.waves.cleared == 0,
          "an arena without waves holds the lock");
}

} // namespace

int main() {
    edges();
    crossing();
    order();
    first();
    return test::verdict("framework-brawl-wave-lock");
}
