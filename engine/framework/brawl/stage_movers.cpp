#include "stage_movers.hpp"
#include "level_objects.hpp"

#include <cstdio>
#include <cstdlib>

#include "platform_world.hpp"

namespace framework::brawl {

namespace {

[[noreturn]] void refuse(const char* why) {
    std::fprintf(stderr, "[brawl] %s\n", why);
    std::abort();
}

} // namespace

void step_stage_movers(StageMovers* m, const BrawlWorld& w) {
    if (m == nullptr) {
        if (w.level != nullptr && !w.level->paths.empty())
            refuse("the level has paths but step_stage got no movers: its platforms would stand still");
        return;
    }
    if (w.level == nullptr || w.platform == nullptr) refuse("step_stage got movers without a level or a platform world");
    if (w.platform->scene.world != &m->world)
        refuse("step_stage got movers whose world is not the platform collision scene: riders would fall through");
    kinematic::step_movers(*m, w.level->paths, w.platform->dt);
}

} // namespace framework::brawl
