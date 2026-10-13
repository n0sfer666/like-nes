#include "stage_movers.hpp"

#include <cstdio>
#include <cstdlib>

#include "platform_world.hpp"
#include "shape.hpp"
#include "units.hpp"

namespace framework::brawl {

namespace {

physics::BodyDesc mover(const scene::Path& path, uint32_t index) {
    physics::BodyDesc d;
    d.key = index + 1;
    d.type = physics::BodyType::Kinematic;
    d.shape = physics::box(fix32::from_int(path.w) / fix32::from_int(2), fix32::from_int(path.h) / fix32::from_int(2));
    d.position = path_point(path, PathFollow{});
    return d;
}

[[noreturn]] void refuse(const char* why) {
    std::fprintf(stderr, "[brawl] %s\n", why);
    std::abort();
}

void step_movers(StageMovers& m, const scene::LevelObjects& level, fix32 dt) {
    if (m.follow.size() != level.paths.size()) refuse("movers were built for a level with another number of paths");
    if (!(fix32{} < dt)) refuse("movers step with a platform dt that is not above 0");
    for (uint32_t i = 0; i < static_cast<uint32_t>(m.follow.size()); ++i) {
        const scene::Path& path = level.paths[i];
        // Загрузчик отказывает выше 32 px за тик, считанных под 60 Гц, а `dt` назначает игра: на шаге короче скорость тела
        // вышла бы за `MAX_SPEED`, и кламп оставлял бы тело позади следования — всё дальше с каждым тиком.
        if (physics::MAX_SPEED * dt < path.speed) refuse("a path speed exceeds MAX_SPEED at this platform dt");
        advance_path(path, m.follow[i]);
        const Vec2 target = path_point(path, m.follow[i]);
        physics::Body& b = m.world.mutate(physics::BodyId{i});
        b.velocity = {(target.x - b.position.x) / dt, (target.y - b.position.y) / dt};
    }
    m.world.step(dt);
}

} // namespace

StageMovers::StageMovers(const scene::LevelObjects& level)
    : world(static_cast<uint32_t>(level.paths.size())), follow(level.paths.size()) {
    world.set_gravity({});
    for (uint32_t i = 0; i < static_cast<uint32_t>(level.paths.size()); ++i) world.add(mover(level.paths[i], i));
}

void step_stage_movers(StageMovers* m, const BrawlWorld& w) {
    if (m == nullptr) {
        if (w.level != nullptr && !w.level->paths.empty())
            refuse("the level has paths but step_stage got no movers: its platforms would stand still");
        return;
    }
    if (w.level == nullptr || w.platform == nullptr) refuse("step_stage got movers without a level or a platform world");
    if (w.platform->scene.world != &m->world)
        refuse("step_stage got movers whose world is not the platform collision scene: riders would fall through");
    step_movers(*m, *w.level, w.platform->dt);
}

} // namespace framework::brawl
