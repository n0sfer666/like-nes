#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_grab_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Grapple;
namespace graphics = framework::graphics;

constexpr fix32 px(int32_t v) { return fix32::from_int(v); }

void test_grab_throws_the_target() {
    Grapple g;
    Body& a = g.reaching(0, 1, 0);
    Body& b = g.r.put(16, -1, 1);
    b.pos.z = px(2);
    a.pos.vx = px(1);
    check(g.hit().count == 1, "control: the grab reaches the push box of the target");
    check(b.react == Reaction::Thrown && b.clip == g.kind().thrown && b.hp == 94 && b.hitstop == 2,
          "a grab deals its damage and throws the target on its clip");
    check(b.pos.x == a.pos.x && b.pos.z == a.pos.z && b.pos.vx == px(3) && b.pos.vy == px(4) && b.facing == 1,
          "the thrown body leaves from the thrower's anchor at the grab's knock, facing the throw");
    check(b.grip.by == a.id && b.grip.team == 0 && b.grip.kind == 0, "the thrown body keeps its thrower's grip");
    check(a.clip == g.kind().throws && a.elapsed == 0 && a.move == g.r.row("grab") && a.hitstop == 2 &&
              a.pos.vx == fix32{},
          "the thrower stops and plays its throw clip under the grab row");
}

void test_grab_goes_through_the_block() {
    Grapple g;
    g.reaching(0, 1, 0);
    Body& b = g.r.put(16, -1, 1);
    b.react = Reaction::Block;
    g.hit();
    check(b.react == Reaction::Thrown, "a grab is not blocked from the front");
}

uint32_t grabs_of(Reaction react, int32_t target_y, int32_t grabber_y) {
    Grapple g;
    g.reaching(0, 1, 0).pos.y = px(grabber_y);
    Body& b = g.r.put(16, -1, 1);
    b.react = react;
    b.pos.y = px(target_y);
    return g.r.collect().count;
}

void test_grab_needs_both_on_the_ground() {
    check(grabs_of(Reaction::None, 0, 0) == 1 && grabs_of(Reaction::Hurt, 0, 0) == 1, "control: a grab on the ground");
    check(grabs_of(Reaction::None, 2, 0) == 0, "a body in the air is not grabbed");
    check(grabs_of(Reaction::None, 0, 2) == 0, "a grab in the air grabs nothing");
    for (const Reaction r : {Reaction::Fall, Reaction::Down, Reaction::Getup, Reaction::Dodge, Reaction::Thrown})
        check(grabs_of(r, 0, 0) == 0, "a falling, lying, rising, rolling or thrown body is not grabbed");
}

uint32_t grabs_with(graphics::Rect16 grasp) {
    std::vector<graphics::ClipSrc> src = test::dummy_clips();
    src[11] = test::dummy_clip("reach", {false, true, true, false}, graphics::CLIP_ONCE, grasp);
    test::Ring r{src, std::string(test::DUMMY_TEXT) + test::GRAB_ROWS};
    Body& a = r.put(0, 1, 0);
    uint16_t reach = 0;
    clip_index(r.arena.clips, "dummy/reach", reach);
    r.play(a, reach);
    a.elapsed = 1;
    r.put(16, -1, 1);
    return r.collect().count;
}

void test_grab_is_judged_on_the_push_box() {
    check(grabs_with(test::GRASP) == 1, "control: a grasp low enough meets the push box");
    check(grabs_with(test::REACH) == 0, "a grasp over the hurt box but above the push box grabs nothing");
}

void test_grab_needs_its_own_clip() {
    for (const bool own : {true, false}) {
        Grapple g;
        Body& a = g.reaching(0, 1, 0);
        if (!own) a.clip = g.kind().thrown;
        g.r.put(8, -1, 1);
        check((g.r.collect().count == 1) == own, "a grab row grabs only from its own clip");
    }
}

void test_two_boxes_of_one_grab_hold() {
    std::vector<graphics::ClipSrc> src = test::dummy_clips();
    for (const std::size_t f : {std::size_t{1}, std::size_t{2}})
        src[11].frames[f].boxes.push_back({graphics::BoxKind::Hit, 1, test::GRASP});
    const std::string grab1 = "move | grab | hit1\nclip | reach\ntype | grab\ndamage | 1\ndepth | 4\nhitstop | 2\n"
                              "hitstun | 0\nknock_x | 3\nknock_y | 4\nhits_down | no\nslide | no\n";
    test::Ring r{src, std::string(test::DUMMY_TEXT) + test::GRAB_ROWS + grab1};
    Body& a = r.put(0, 1, 0);
    clip_index(r.arena.clips, "dummy/reach", a.clip);
    a.move = r.row("grab");
    a.elapsed = 1;
    Body& b = r.put(16, -1, 1);
    HitEvents e = r.collect();
    apply_hits(r.pool, r.arena.kinds, e);
    check(e.count == 2 && b.react == Reaction::Thrown && b.hp == 100 - 6 - 1,
          "two boxes of one grab on one target are one grab, not a double grab");
}

void test_friendly_grab_needs_friendly_fire() {
    for (const bool ff : {false, true}) {
        Grapple g;
        g.reaching(0, 1, 0);
        Body& b = g.r.put(16, -1, 0);
        g.hit(ff);
        check((b.react == Reaction::Thrown) == ff, "a teammate is grabbed only with friendly fire on");
    }
}

} // namespace

int main() {
    std::printf("brawl: grab and throw\n");
    test_grab_throws_the_target();
    test_grab_goes_through_the_block();
    test_grab_needs_both_on_the_ground();
    test_grab_is_judged_on_the_push_box();
    test_grab_needs_its_own_clip();
    test_two_boxes_of_one_grab_hold();
    test_friendly_grab_needs_friendly_fire();
    return test::verdict("framework-brawl-grab");
}
