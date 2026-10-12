#include "framework_scene_stack_fixture.hpp"

namespace stack_fixture {

namespace {

using sc::SceneResult;

void arm(Probe& sw, sc::SceneStack& s, Cmd cmd, const sc::Scene& next) {
    sw.stack = &s;
    sw.cmd = cmd;
    sw.next = next;
    sw.got = SceneResult::Ok;
}

void disarm(Probe& sw) {
    sw.stack = nullptr;
    sw.cmd = Cmd::Replace;
    sw.next = sc::Scene{};
    sw.got = SceneResult::Ok;
}

void test_refused(Probe& sw, Probe& b) {
    sc::SceneStack s;
    g_trace.clear();
    arm(sw, s, Cmd::Replace, scene(b));
    s.push(scene(sw, true));
    tick(s, 0);
    check(sw.got == SceneResult::Rollback && s.pending() == 0 && g_trace == "s",
          "a system of a rollback scene gets Rollback and queues nothing");
    tick(s, 1);
    check(s.depth() == 1 && s.top()->user == &sw && g_trace == "ss", "the refused replace never applies");
    arm(sw, s, Cmd::Replace, sc::Scene{});
    tick(s, 2);
    check(sw.got == SceneResult::Rollback, "the rollback refusal of replace comes before the check of the scene");
    arm(sw, s, Cmd::Push, sc::Scene{});
    tick(s, 3);
    check(sw.got == SceneResult::Rollback && s.pending() == 0, "push from a rollback scene is refused first");
    arm(sw, s, Cmd::Pop, sc::Scene{});
    tick(s, 4);
    check(sw.got == SceneResult::Rollback && s.pending() == 0, "pop from a rollback scene is refused");
    disarm(sw);
    check(s.replace(scene(b)) == SceneResult::Ok, "game code after run issues the same replace");
    tick(s, 5);
    check(s.top()->user == &b && g_trace == "sssssb", "the replace of the game code applies on the next tick");
}

void test_full(Probe& a, Probe& sw) {
    sc::SceneStack s;
    for (std::size_t i = 1; i < sc::SCENE_STACK_CAPACITY; ++i) s.push(scene(a));
    s.push(scene(sw, true));
    arm(sw, s, Cmd::Push, scene(a));
    tick(s, 0);
    check(s.depth() == sc::SCENE_STACK_CAPACITY && sw.got == SceneResult::Rollback,
          "push from a rollback scene on a full stack is Rollback, not Overflow");
    disarm(sw);
}

void test_resim(Probe& sw, Probe& b) {
    sc::SceneStack s;
    g_trace.clear();
    s.push(scene(sw));
    tick(s, 0);
    arm(sw, s, Cmd::NestedRun, scene(b));
    check(s.replace(scene(b)) == SceneResult::Ok, "game code queues a replace for the next tick");
    s.resim(framework::Tick{0, fix32::from_int(1)});
    check(sw.got == SceneResult::Rollback && g_trace == "ss", "resim refuses scene commands whatever the scene flag");
    check(s.pending() == 1 && s.top()->user == &sw, "resim and a run nested in it leave the queue unapplied");
    disarm(sw);
    check(s.push(scene(b)) == SceneResult::Ok, "game code after resim gets Ok");
    tick(s, 1);
    check(s.depth() == 2 && s.top()->user == &b && g_trace == "ssb", "the queued commands apply on the next run");
}

void test_nested(Probe& sw, Probe& b) {
    sc::SceneStack s;
    g_trace.clear();
    s.push(scene(sw, true));
    arm(sw, s, Cmd::NestedRun, scene(b));
    tick(s, 0);
    check(sw.got == SceneResult::Rollback && s.pending() == 0 && g_trace == "s",
          "a run nested in a rollback scene runs nothing and leaves the refusal on");
    disarm(sw);
}

} // namespace

void test_rollback(Probe& a, Probe& b, Probe& sw) {
    test_refused(sw, b);
    test_full(a, sw);
    test_resim(sw, b);
    test_nested(sw, b);
}

} // namespace stack_fixture
