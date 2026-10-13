#include <cstdio>

#include "body_hash.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_shot_fixture.hpp"

// Снаряд в belt (спека #25, В8г): летит своей скоростью, `life` убывает в тике и снимает его молча,
// бьёт одного — первого по `seq`, щадит владельца и свою команду, бокс вида с клипом — из кадра;
// в platform мир судится боксом удара, без него — точкой.
// Отработавший снаряд лежит с `life == 0` до `drop_spent` в начале следующего тика.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;

void flies_and_expires() {
    Shots w;
    const EntId owner = w.fighter(100, 0);
    const EntId shot = w.shoot(owner, aim(110, 4), 3);
    const Body b = *w.find(shot);
    check(b.facing == 1 && b.team == 0 && b.owner == owner && b.kind == SHOT && b.life == 3 &&
              b.clip == w.kinds[SHOT].moves[0].clip && b.move == 0 && b.hp == 0,
          "the shot does not take its owner's side, its kind's clip and its life");
    w.step();
    check(w.find(shot)->pos.x == at(114) && w.find(shot)->life == 2,
          "the shot does not fly by its velocity or count its life down");
    w.step();
    check(w.flying(shot), "the shot is gone before its life runs out");
    w.step();
    check(!w.flying(shot) && w.events.count == 0 && w.deaths.count == 0,
          "an expired shot stays or leaves a fact");
    const EntId back = w.shoot(owner, aim(90, -4), 5);
    const EntId still = w.shoot(w.fighter(200, 1, LANE, -1), DepthBody{at(190), at(LANE), {}, {}, at(1), {}}, 5);
    check(w.find(back)->facing == -1 && w.find(still)->facing == -1 && w.find(still)->team == 1,
          "the shot faces neither its flight nor, standing in x, its owner");
}

void hits_the_first_by_seq() {
    Shots w;
    const EntId owner = w.fighter(100, 0);
    const EntId early = w.fighter(154, 1, 270);
    const EntId late = w.fighter(150, 1, 230);
    const EntId shot = w.shoot(owner, aim(146, 4), 20);
    w.step();
    check(w.events.count == 1 && w.events.at[0].attacker == shot && w.events.at[0].target == early &&
              w.events.at[0].kept,
          "a shot touching two bodies does not hit only the first in seq");
    check(w.hp(early) == 90 && w.hp(late) == 100 && w.find(early)->react == Reaction::Hurt,
          "the shot does not deal its strike to the first body alone");
    check(!w.flying(shot) && w.find(shot) != nullptr,
          "a shot that hit flies on or leaves its tick's events without it");
    w.step();
    check(w.find(shot) == nullptr, "a spent shot is not dropped at the next tick");
}

void spares_its_side() {
    Shots w;
    const EntId owner = w.fighter(100, 0);
    const EntId mate = w.fighter(140, 0);
    const EntId shot = w.shoot(owner, aim(96, 4), 30);
    for (int t = 0; t < 20; ++t) w.step();
    check(w.hp(owner) == 100 && w.hp(mate) == 100 && w.flying(shot), "the shot hits its own team");
    Shots ff;
    ff.friendly_fire = true;
    const EntId self = ff.fighter(100, 0);
    const EntId ally = ff.fighter(140, 0);
    ff.shoot(self, aim(96, 4), 30);
    for (int t = 0; t < 20; ++t) ff.step();
    check(ff.hp(self) == 100 && ff.hp(ally) == 90, "under friendly fire the shot spares not its owner or hits no ally");
}

void passes_other_shots() {
    Shots w;
    const EntId ours = w.shoot(w.fighter(0, 0), aim(100, 4), 25);
    const EntId theirs = w.shoot(w.fighter(380, 1), aim(200, -4), 25, CLIP_SHOT);
    for (int t = 0; t < 20; ++t) w.step();
    check(w.flying(ours) && w.flying(theirs) && w.find(ours)->pos.x == at(180),
          "crossing shots hit each other");
}

void clip_kind_uses_the_frame() {
    Shots w;
    const EntId owner = w.fighter(100, 0);
    const EntId far = w.fighter(300, 1);
    const EntId near = w.fighter(150, 1);
    w.shoot(owner, aim(130, 0), 2, CLIP_SHOT);
    w.step();
    check(w.events.count == 1 && w.events.at[0].target == near && w.hp(near) == 95 && w.hp(far) == 100,
          "a shot with a clip does not strike with its frame box and its move's numbers");
    Shots off;
    off.kinds[CLIP_SHOT].moves[0].strike.box = 1;
    const EntId target = off.fighter(150, 1);
    off.shoot(off.fighter(100, 0), aim(130, 0), 2, CLIP_SHOT);
    off.step();
    check(off.events.count == 0 && off.hp(target) == 100, "a shot strikes with a box its strike does not name");
}

void judged_by_its_strike_box() {
    Shots w({{560, 200, 580, 240}});
    const EntId owner = w.fighter(500, 0, FLOOR);
    w.step();
    const EntId reach = w.shoot(owner, aim(515, 20, FLOOR, 8), 30, CLIP_SHOT);
    const uint32_t t = w.lifetime(reach);
    check(t < 30 && at(539) < w.find(reach)->pos.x && !(at(541) < w.find(reach)->pos.x),
          "a platform shot is not judged by its strike box in the frame");
    Shots off({{560, 200, 580, 240}});
    off.kinds[CLIP_SHOT].moves[0].strike.box = 7;
    const EntId by = off.fighter(500, 0, FLOOR);
    off.step();
    const EntId shot = off.shoot(by, aim(510, 4, FLOOR, 8), 30, CLIP_SHOT);
    const uint32_t life = off.lifetime(shot);
    check(life < 30 && at(558) < off.find(shot)->pos.x && !(at(559) < off.find(shot)->pos.x),
          "a platform shot without its strike box in the frame is not judged by a point");
}

enum class Forget : uint8_t { Nothing, Life };

struct Replay {
    int32_t straight = 0;
    int32_t again = 0;
    uint64_t straight_hash = 0;
    uint64_t again_hash = 0;
};

Replay resim(Forget forget) {
    Shots w;
    const EntId foe = w.fighter(200, 1);
    w.shoot(w.fighter(100, 0), aim(110, 4), 40);
    for (int t = 0; t < 5; ++t) w.step();
    const BodyPool pool = w.pool();
    const StageSections sections = w.sec.s.sections;
    const StageCamera camera = w.sec.s.camera;
    Replay r;
    for (int t = 0; t < 30; ++t) w.step();
    r.straight = w.hp(foe);
    r.straight_hash = state_hash(w.pool());
    w.pool() = pool;
    w.sec.s.sections = sections;
    w.sec.s.camera = camera;
    if (forget == Forget::Life)
        for (uint32_t i = 0; i < w.pool().count; ++i)
            if (role_of(w.pool().bodies[i], w.kinds) == Role::Projectile) w.pool().bodies[i].life = 0;
    for (int t = 0; t < 30; ++t) w.step();
    r.again = w.hp(foe);
    r.again_hash = state_hash(w.pool());
    return r;
}

void snapshot() {
    const Replay kept = resim(Forget::Nothing);
    check(kept.straight == 90 && kept.again == 90 && kept.straight_hash == kept.again_hash,
          "a restored pool does not replay the shot");
    const Replay lost = resim(Forget::Life);
    check(lost.straight == 90 && lost.again == 100, "a pool restored without life replays the shot");
}

} // namespace

int main() {
    flies_and_expires();
    hits_the_first_by_seq();
    spares_its_side();
    passes_other_shots();
    clip_kind_uses_the_frame();
    judged_by_its_strike_box();
    snapshot();
    return verdict("framework-brawl-projectile");
}
