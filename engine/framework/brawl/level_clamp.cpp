#include "level_clamp.hpp"

#include "camera.hpp"

namespace framework::brawl {

namespace {

Vec2 clamp_into(Vec2 c, const scene::LevelRect& r, Vec2 half) {
    return {graphics::clamp_axis(c.x, r.x0, r.x1, half.x), graphics::clamp_axis(c.y, r.y0, r.y1, half.y)};
}

} // namespace

Vec2 clamp_level(Vec2 c, int16_t arena, const scene::LevelObjects& level, Vec2 half) {
    for (const scene::Arena& a : level.arenas)
        if (arena != NO_ARENA && a.id == arena) c = clamp_into(c, a.area.rect, half);
    if (level.bounds) c = clamp_into(c, level.bounds->rect, half);
    return c;
}

} // namespace framework::brawl
