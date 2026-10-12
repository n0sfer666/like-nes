#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

#include "scene_stack.hpp"

namespace stack_fixture {

namespace sc = framework::scene;

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

inline std::string g_trace;

enum class Cmd : uint8_t { Replace, Push, Pop, NestedRun };

struct Probe {
    framework::Schedule schedule;
    char tag = '?';
    sc::SceneStack* stack = nullptr;
    sc::Scene next{};
    Cmd cmd = Cmd::Replace;
    sc::SceneResult got = sc::SceneResult::Ok;
};

inline void mark(void* user, const framework::Tick&) { g_trace += static_cast<Probe*>(user)->tag; }

inline void switch_away(void* user, const framework::Tick&) {
    Probe* p = static_cast<Probe*>(user);
    sc::SceneStack* s = p->stack;
    if (s == nullptr) return;
    switch (p->cmd) {
    case Cmd::Replace: p->got = s->replace(p->next); break;
    case Cmd::Push: p->got = s->push(p->next); break;
    case Cmd::Pop: p->got = s->pop(); break;
    case Cmd::NestedRun:
        p->stack = nullptr;
        s->run(framework::Tick{0, fix32::from_int(1)});
        p->stack = s;
        p->got = s->replace(p->next);
        break;
    }
}

inline void build(Probe& p, char tag, bool switches = false) {
    p.tag = tag;
    framework::SystemDesc first;
    first.name = "switch";
    first.stage = framework::Stage::PreSim;
    first.fn = &switch_away;
    first.user = &p;
    framework::SystemDesc second;
    second.name = "mark";
    second.stage = framework::Stage::Sim;
    second.fn = &mark;
    second.user = &p;
    if (switches) p.schedule.add(first);
    p.schedule.add(second);
    p.schedule.build();
}

inline sc::Scene scene(Probe& p, bool rollback = false) { return sc::Scene{&p.schedule, &p, rollback}; }

inline void tick(sc::SceneStack& s, uint32_t index) { s.run(framework::Tick{index, fix32::from_int(1)}); }

void test_rollback(Probe& a, Probe& b, Probe& sw);

} // namespace stack_fixture
