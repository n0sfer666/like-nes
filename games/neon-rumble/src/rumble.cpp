#include "rumble.hpp"

#include <cstdio>

namespace rumble {

namespace {

void count_tick(void* user, const framework::Tick&) {
    Scene& scene = *static_cast<Scene*>(user);
    ++scene.ticks;
    scene.brawl->step();
}

} // namespace

bool Scene::init(Brawl& fight) {
    brawl = &fight;
    framework::SystemDesc tick;
    tick.name = "count_tick";
    tick.stage = framework::Stage::Sim;
    tick.fn = count_tick;
    tick.user = this;
    if (!schedule.add(tick)) return false;
    const framework::BuildResult r = schedule.build();
    if (r != framework::BuildResult::Ok) {
        std::fprintf(stderr, "neon-rumble: schedule build failed: %s\n", framework::build_reason(r));
        return false;
    }
    return true;
}

void Scene::step(uint32_t index) {
    framework::Tick t;
    t.index = index;
    t.dt = fix32::from_int(1) / fix32::from_int(60);
    schedule.run(t);
}

} // namespace rumble
