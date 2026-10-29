#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_rope_fixture.hpp"

// Захват троса (спека #25, В8в): досягаемость от рук до конца, отказы — на земле, второму бойцу, в
// реакции и в стопе удара, второй трос седоку, подвес в стене — и скорость тела, перешедшая в трос.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::Vec2;

bool takes(Vec2 offset, const Command& c = grab()) {
    Swing s;
    const EntId id = s.fighter(500);
    s.hang_hands(id, s.end() + offset);
    s.step({c});
    const Vec2 hands = s.hands(id);
    return s.ropes.at[0].rider == id && hands.x == s.end().x && hands.y == s.end().y;
}

void reach() {
    check(takes({at(0), at(16)}) && takes({at(-16), at(0)}), "a hand on the reach edge does not take the rope");
    check(takes({at(11), at(-11)}), "a hand inside the reach on a diagonal does not take the rope");
    check(!takes({at(0), fix32::from_raw(at(16).raw + 1)}) && !takes({at(12), at(12)}),
          "a hand past the reach takes the rope");
    check(!takes({at(0), at(4)}, go(0)) && !takes({at(0), at(4)}, Command{}), "the rope is taken without the grab button");
}

// Уступ на 8 px ниже ног висящего: стоящий на нём держит руки в досягаемости, и путь корпуса к
// подвесу свободен — отказ даёт только земля под ногами.
void grounded() {
    Swing ledge({{592, 196, 608, 200}});
    const EntId stand = ledge.fighter(500);
    ledge.hang_hands(stand, ledge.end());
    ledge.idle(8);
    const fix32 below = ledge.hands(stand).y - ledge.end().y;
    check(ledge.section(stand).c.on_ground && at(0) < below && !(at(16) < below),
          "the fighter does not stand on the ledge within the reach");
    ledge.step({grab()});
    check(ledge.ropes.at[0].rider == EntId{}, "a fighter on the ground takes the rope");
}

void refusals() {
    Swing pair;
    const EntId first = pair.fighter(500);
    const EntId second = pair.w.body(520);
    pair.step({});
    pair.hang_hands(first, pair.end());
    pair.hang_hands(second, pair.end());
    pair.step({grab(), grab()});
    pair.hang_hands(second, pair.end());
    pair.step({grab(), grab()});
    check(pair.ropes.at[0].rider == first, "the second fighter takes a rope with a rider");
    Swing hurt;
    const EntId id = hurt.fighter(500);
    hurt.hang_hands(id, hurt.end());
    hurt.w.get(id).react = Reaction::Hurt;
    hurt.step({grab()});
    check(hurt.ropes.at[0].rider == EntId{}, "a fighter in a reaction takes the rope");
    Swing frozen;
    const EntId stopped = frozen.fighter(500);
    frozen.hang_hands(stopped, frozen.end());
    frozen.w.get(stopped).hitstop = 2;
    frozen.step({grab()});
    check(frozen.ropes.at[0].rider == EntId{}, "a fighter in hitstop takes the rope");
    Swing crate;
    const EntId thing = crate.fighter(500);
    crate.hang_hands(thing, crate.end());
    crate.w.s.arena.kinds[0].role = Role::Prop;
    crate.step({grab()});
    check(crate.ropes.at[0].rider == EntId{}, "a prop takes the rope");
    Swing two({}, {rope(PIVOT_X, PIVOT_Y, LENGTH), rope(PIVOT_X + 8, PIVOT_Y, LENGTH)});
    const EntId both = two.fighter(500);
    two.hang_hands(both, two.end());
    two.step({grab()});
    two.step({grab()});
    check(two.ropes.at[0].rider == both && two.ropes.at[1].rider == EntId{}, "a rider takes a second rope");
}

// Стена слева от отвеса входит в корпус висящего; боец подлетает справа, руки — на краю досягаемости.
void walled() {
    Swing s({{588, 160, 598, 236}});
    const EntId id = s.fighter(500);
    s.hang_hands(id, s.end() + Vec2{at(16), at(0)});
    s.step({grab()});
    check(s.ropes.at[0].rider == EntId{}, "a rope whose hang point is inside a wall is taken");
}

// В отвесе касательная — ось +X: скорость троса — горизонтальная скорость тела за тик.
void carries_speed() {
    Swing s;
    const EntId id = s.fighter(500);
    s.hang_hands(id, s.end());
    SectionBody& b = s.section(id);
    b.c.velocity = {at(120), at(0)};
    mirror_body(b.c, hull_foot(s.w.platform), s.w.platform.dt, s.w.get(id).pos);
    const fix32 vx = s.w.get(id).pos.vx;
    s.step({grab()});
    check(s.ropes.at[0].rider == id && at(1) < vx && s.ropes.at[0].swing.speed == vx,
          "the grab does not turn the body speed into the rope speed");
}

} // namespace

int main() {
    reach();
    grounded();
    refusals();
    walled();
    carries_speed();
    return verdict("framework-brawl-rope-grab");
}
