#include "movers.hpp"

#include <cstdio>
#include <cstdlib>

#include "shape.hpp"
#include "units.hpp"

namespace framework::kinematic {

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
    std::fprintf(stderr, "[kinematic] %s\n", why);
    std::abort();
}

} // namespace

Movers::Movers(std::span<const scene::Path> paths)
    : world(static_cast<uint32_t>(paths.size())), follow(paths.size()) {
    world.set_gravity({});
    for (uint32_t i = 0; i < static_cast<uint32_t>(paths.size()); ++i) world.add(mover(paths[i], i));
}

void step_movers(Movers& m, std::span<const scene::Path> paths, fix32 dt) {
    if (m.follow.size() != paths.size()) refuse("movers were built for a level with another number of paths");
    if (!(fix32{} < dt)) refuse("movers step with a platform dt that is not above 0");
    for (uint32_t i = 0; i < static_cast<uint32_t>(m.follow.size()); ++i) {
        const scene::Path& path = paths[i];
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


} // namespace framework::kinematic
