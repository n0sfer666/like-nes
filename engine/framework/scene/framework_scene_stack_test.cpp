#include <cstdio>
#include <string>

#include "framework_scene_stack_fixture.hpp"

namespace {

namespace sc = framework::scene;
using sc::SceneResult;
using namespace stack_fixture;

void test_deferred(Probe& a, Probe& b) {
    sc::SceneStack s;
    g_trace.clear();
    check(s.push(scene(a, true)) == SceneResult::Ok && s.depth() == 0 && s.pending() == 1,
          "push is queued, the stack does not change at the call");
    tick(s, 0);
    check(s.depth() == 1 && s.pending() == 0 && g_trace == "a", "queued push applies at the start of the next tick");
    check(s.top()->user == &a && s.top()->rollback, "scene keeps its user and its rollback flag");
    check(s.push(scene(b)) == SceneResult::Ok && s.pop() == SceneResult::Ok && s.depth() == 1,
          "push then pop in one tick are both accepted against the projected depth");
    tick(s, 1);
    check(s.depth() == 1 && s.top()->user == &a && g_trace == "aa", "push then pop applied in order leaves the stack as it was");
}

void test_order(Probe& a, Probe& b, Probe& c) {
    sc::SceneStack s;
    g_trace.clear();
    s.push(scene(a));
    s.push(scene(b));
    tick(s, 0);
    check(s.depth() == 2 && s.at(0)->user == &a && s.at(2) == nullptr && s.top()->user == &b && g_trace == "b",
          "two pushes apply in submission order and only the top scene runs");
    check(s.replace(scene(c)) == SceneResult::Ok, "replace of a non-empty stack is accepted");
    tick(s, 1);
    check(s.depth() == 2 && s.at(0)->user == &a && s.top()->user == &c && g_trace == "bc",
          "replace swaps the top without touching the scene below");
    check(s.pop() == SceneResult::Ok, "pop of a non-empty stack is accepted");
    tick(s, 2);
    check(s.depth() == 1 && s.top()->user == &a && g_trace == "bca", "pop uncovers the scene below in the same tick");
}

void test_mid_tick(Probe& sw, Probe& b) {
    sc::SceneStack s;
    g_trace.clear();
    sw.stack = &s;
    sw.next = scene(b);
    s.push(scene(sw));
    tick(s, 0);
    check(s.depth() == 1 && s.top()->user == &sw && s.pending() == 1 && g_trace == "s" && sw.got == SceneResult::Ok,
          "replace issued by a system mid-step does not cut the step short");
    sw.stack = nullptr;
    tick(s, 1);
    check(s.top()->user == &b && g_trace == "sb", "replace issued mid-step applies at the start of the next tick");
}

void test_overflow(Probe& a, Probe& b) {
    sc::SceneStack s;
    for (std::size_t i = 0; i + 1 < sc::SCENE_STACK_CAPACITY; ++i) s.push(scene(a));
    tick(s, 0);
    check(s.push(scene(a)) == SceneResult::Ok, "push up to the capacity is accepted");
    check(s.push(scene(b)) == SceneResult::Overflow && s.pending() == 1,
          "push past the capacity counts queued pushes and is refused without queueing");
    tick(s, 1);
    check(s.depth() == sc::SCENE_STACK_CAPACITY && s.top()->user == &a, "refused push leaves the stack at the capacity");
    check(s.replace(scene(b)) == SceneResult::Ok, "replace of a full stack is accepted");
}

void test_empty(Probe& a) {
    sc::SceneStack s;
    g_trace.clear();
    check(s.pop() == SceneResult::Empty && s.pending() == 0, "pop of an empty stack is refused");
    check(s.replace(scene(a)) == SceneResult::Empty && s.pending() == 0, "replace of an empty stack is refused");
    tick(s, 0);
    check(s.depth() == 0 && s.top() == nullptr && g_trace.empty(), "an empty stack runs nothing");
    s.push(scene(a));
    s.pop();
    check(s.pop() == SceneResult::Empty && s.pending() == 2, "pop after a queued push and pop is refused");
    check(s.replace(scene(a)) == SceneResult::Empty && s.pending() == 2, "replace after a queued push and pop is refused");
}

void test_replace_projected(Probe& a, Probe& b) {
    sc::SceneStack s;
    g_trace.clear();
    check(s.push(scene(a)) == SceneResult::Ok && s.replace(scene(b)) == SceneResult::Ok,
          "replace after a queued push on an empty stack is accepted");
    tick(s, 0);
    check(s.depth() == 1 && s.top()->user == &b && g_trace == "b", "queued push then replace leaves the replacing scene");
}

void test_bad_scene(Probe& a) {
    sc::SceneStack s;
    framework::Schedule unbuilt;
    check(s.push(sc::Scene{}) == SceneResult::BadScene && s.pending() == 0, "scene without a schedule is refused");
    check(s.push(sc::Scene{&unbuilt, nullptr, false}) == SceneResult::BadScene && s.pending() == 0,
          "scene with an unbuilt schedule is refused");
    s.push(scene(a));
    check(s.replace(sc::Scene{}) == SceneResult::BadScene && s.pending() == 1, "replace with a bad scene is refused");
}

void test_queue(Probe& a) {
    sc::SceneStack s;
    for (std::size_t i = 0; i < sc::SCENE_QUEUE_CAPACITY / 2; ++i) {
        s.push(scene(a));
        s.pop();
    }
    check(s.push(scene(a)) == SceneResult::QueueFull && s.pending() == sc::SCENE_QUEUE_CAPACITY,
          "a command past the queue capacity is refused");
    check(s.apply() == sc::SCENE_QUEUE_CAPACITY && s.depth() == 0 && s.push(scene(a)) == SceneResult::Ok,
          "applying the queue frees it for the next tick");
    check(s.pop() == SceneResult::Ok && s.pop() == SceneResult::Empty,
          "a refused command does not move the projected depth");
}

void test_reasons() {
    const SceneResult all[] = {SceneResult::Ok,    SceneResult::BadScene,  SceneResult::Overflow,
                               SceneResult::Empty, SceneResult::QueueFull, SceneResult::Rollback};
    bool distinct = true;
    for (const SceneResult x : all)
        for (const SceneResult y : all)
            if (x != y && std::string(sc::scene_reason(x)) == sc::scene_reason(y)) distinct = false;
    check(distinct && std::string(sc::scene_reason(SceneResult::Overflow)) != "unknown", "every refusal has its own text");
}

} // namespace

int main() {
    Probe a, b, c, sw;
    build(a, 'a');
    build(b, 'b');
    build(c, 'c');
    build(sw, 's', true);
    test_deferred(a, b);
    test_order(a, b, c);
    test_mid_tick(sw, b);
    test_overflow(a, b);
    test_empty(a);
    test_replace_projected(a, b);
    test_bad_scene(a);
    test_queue(a);
    test_rollback(a, b, sw);
    test_reasons();
    std::printf("framework-scene-stack: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
