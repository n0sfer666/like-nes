#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_path_fixture.hpp"
#include "framework_brawl_rope_fixture.hpp"

// Трос среди мира (спека #25, В8в): лифт на пути седока роняет его, поводок, сдвинувший седока, —
// тоже; односторонний уступ держит седока только сверху; удар с троса гасит раскачку; отпустивший
// трос в том же тике не берёт соседний; седок, убранный из секции, падает.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::Vec2;

EntId ride(Swing& s, int32_t degrees) {
    const EntId id = s.fighter(500);
    s.ropes.at[0].swing.arc = deflection(at(LENGTH), degrees);
    s.hang_hands(id, s.end());
    s.step({grab()});
    return id;
}

bool dropped(const Swing& s) { return s.ropes.at[0].rider == EntId{} && s.ropes.at[0].cooldown == ROPE_COOLDOWN; }

// Лифт стоит справа от отвеса на высоте корпуса: седок с −30° несётся в него на ~10°.
void lift() {
    Swing s;
    s.w.s.level.paths.push_back(path({{630, 180}}, 1, false));
    StageMovers m(s.w.s.level);
    s.w.platform.scene.world = &m.world;
    s.movers = &m;
    const EntId id = ride(s, -30);
    bool rode = s.ropes.at[0].rider == id;
    uint32_t t = 0;
    while (s.ropes.at[0].rider == id && t++ < 60) s.step({});
    check(rode && dropped(s) && fix32{} < s.ropes.at[0].swing.speed && s.section(id).c.position.x < at(606),
          "a lift in the swing stops the rope or carries the rider into it");
}

bool leashed(bool across) {
    Swing s;
    const EntId id = ride(s, 30);
    s.idle(5);
    const bool rode = s.ropes.at[0].rider == id;
    DepthBody& pos = s.w.get(id).pos;
    if (across) pos.x = pos.x + at(1);
    else pos.z = pos.z + at(1);
    s.step({});
    return rode && dropped(s);
}

void leash() { check(leashed(true) && leashed(false), "a rider moved off the hang point stays on the rope"); }

// Односторонний уступ в тайл толщиной справа от отвеса, верх на y 176: седок с −45° проходит его
// сбоку и снизу, а на обратном ходе встаёт на него сверху — трос держит, как держит бойца.
void oneway() {
    Swing s;
    s.w.grid.fill(cell(640, 0), cell(176, GRID_TOP), cell(700, 0), cell(180, GRID_TOP),
                  framework::tilemap::TILE_SOLID | framework::tilemap::TILE_ONEWAY);
    const EntId id = ride(s, -45);
    const fix32 half = hull_half(s.w.platform).x;
    fix32 reached = at(0);
    for (uint32_t t = 0; t < 120; ++t) {
        s.step({});
        reached = framework::max_fix(reached, s.hands(id).x + half);
    }
    const fix32 feet = s.section(id).c.position.y + hull_half(s.w.platform).y;
    check(s.ropes.at[0].rider == id && at(660) < reached,
          "a one-way ledge stops the rider from below or from the side");
    check(fix32{} < s.ropes.at[0].swing.arc && s.ropes.at[0].swing.speed == fix32{} && !(at(176) < feet) && at(175) < feet,
          "a one-way ledge does not hold the rider from above");
}

void strike_holds() {
    Swing s;
    const EntId id = ride(s, 0);
    s.step({strike(s.w.s.arena, s.w.s.arena.jab)});
    const int32_t facing = s.w.get(id).facing;
    s.step({go(-facing)});
    s.step({go(-facing)});
    const Body& b = s.w.get(id);
    check(b.move < s.w.s.arena.kinds[b.kind].move_count, "the jab does not run on the rope");
    check(s.ropes.at[0].rider == id && b.facing == facing && s.ropes.at[0].swing.speed == fix32{} &&
              s.ropes.at[0].swing.arc == fix32{},
          "a striking rider pumps or turns the rope");
}

void release_tick() {
    Swing s({}, {rope(PIVOT_X, PIVOT_Y, LENGTH), rope(PIVOT_X + 8, PIVOT_Y, LENGTH)});
    const EntId id = ride(s, 0);
    const bool rode = s.ropes.at[0].rider == id;
    s.step({leap()});
    check(rode && dropped(s) && s.ropes.at[1].rider == EntId{},
          "the released rider takes the next rope in the same tick");
}

void unsectioned() {
    Swing s;
    const EntId id = ride(s, 30);
    const bool rode = s.ropes.at[0].rider == id;
    s.w.s.sections.drop(id);
    s.step({});
    check(rode && dropped(s), "a rider out of the platform section stays on the rope");
}

} // namespace

int main() {
    lift();
    leash();
    oneway();
    strike_holds();
    release_tick();
    unsectioned();
    return verdict("framework-brawl-rope-world");
}
