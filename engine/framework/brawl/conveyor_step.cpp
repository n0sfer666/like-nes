#include "conveyor_step.hpp"

#include "depth_step.hpp"
#include "scene_objects.hpp"

namespace framework::brawl {

namespace {

bool within(fix32 v, fix32 lo, fix32 hi) { return !(v < lo) && v < hi; }

} // namespace

fix32 conveyor_under(const Body& b, const scene::LevelObjects* level) {
    if (level == nullptr || !(b.grip.by == EntId{})) return {};
    for (const scene::Conveyor& c : level->conveyors)
        if (within(b.pos.x, c.area.rect.x0, c.area.rect.x1) && within(b.pos.z, c.area.rect.y0, c.area.rect.y1))
            return c.speed;
    return {};
}

void ride_belt(DepthBody& d, fix32 belt, const DepthFloor& f) {
    if (belt == fix32{} || !grounded(d)) return;
    d.x = move_x(f, d.x, d.z, d.x + belt);
}

} // namespace framework::brawl
