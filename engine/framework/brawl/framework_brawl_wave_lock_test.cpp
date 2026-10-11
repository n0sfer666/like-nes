#include <utility>

#include "framework_brawl_check.hpp"
#include "framework_brawl_wave_fixture.hpp"

namespace {

using namespace framework::brawl;
using framework::scene::LevelObjects;
using framework::scene::SectionMode;
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

// Арена 0 по `y` от 0 до 600 — вид от 108 до 492; секция одна на весь уровень.
bool locks_y(SectionMode mode, int32_t y, int32_t y0 = 0, int32_t y1 = 600) {
    LevelObjects l;
    l.arenas.push_back({area(400, y0, 1000, y1), 0});
    l.sections.push_back({area(0, 0, 2000, 600), mode});
    Waves w(700, l);
    w.s.camera.center.y = at(y);
    w.step();
    return w.facts.locked;
}

void platform_y() {
    check(!locks_y(SectionMode::Platform, 650) && !locks_y(SectionMode::Platform, -1),
          "a platform arena locks with the view at another height");
    check(locks_y(SectionMode::Platform, 600) && locks_y(SectionMode::Platform, 0),
          "a platform arena misses the lock with the view on its y edge");
    check(locks_y(SectionMode::Platform, 230, 100, 316),
          "a view-high platform arena misses the lock off its y target point");
    check(locks_y(SectionMode::Belt, 700), "a belt arena judges y");
}

// Арена 0 по `x` от 1100 до 1400 в platform: вид накрывает её при центре от 1208 до 1292. Первый шаг
// пересекает цель на другой высоте, второй — `y` уже в пролёте.
LevelObjects narrow() {
    LevelObjects l;
    l.arenas.push_back({area(1100, 0, 1400, 600), 0});
    l.sections.push_back({area(0, 0, 2000, 600), SectionMode::Platform});
    return l;
}

std::pair<bool, bool> narrow_late(int32_t x, int32_t y) {
    Waves w(1000, narrow());
    w.s.camera.center = {at(1300), at(650)};
    w.step();
    const bool first = w.facts.locked;
    w.s.camera.center = {at(x), at(y)};
    w.step();
    return {first, w.facts.locked};
}

void platform_x() {
    check(narrow_late(1280, 300) == std::pair{false, true} && narrow_late(1208, 300) == std::pair{false, true},
          "a narrow platform arena passed at another height never locks");
    Waves leap(1200, narrow());
    leap.s.camera.center = {at(1300), at(300)};
    leap.step();
    check(leap.facts.locked, "a narrow platform arena stepped over past its cover window never locks");
    check(narrow_late(1293, 300) == std::pair{false, false} && narrow_late(1280, 601) == std::pair{false, false},
          "a narrow platform arena locks with the view off it");
    LevelObjects l;
    l.arenas.push_back({area(400, 0, 1000, 600), 0});
    l.sections.push_back({area(0, 0, 2000, 600), SectionMode::Platform});
    Waves wide(500, l);
    wide.s.camera.center = {at(591), at(300)};
    wide.step();
    check(!wide.facts.locked, "a wide platform arena locks short of its edge target");
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
    platform_y();
    platform_x();
    order();
    first();
    return test::verdict("framework-brawl-wave-lock");
}
