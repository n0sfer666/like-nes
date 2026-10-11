#include "section_exit.hpp"

#include "collision.hpp"
#include "hit_geometry.hpp"
#include "platform_body.hpp"

namespace framework::brawl {

namespace {

bool near(fix32 a, fix32 b, fix32 reach) { return abs_fix(a - b) < reach; }

fix32 depth_of(const Body& b, const BrawlWorld& w) { return b.kind < w.kinds.size() ? w.kinds[b.kind].depth : fix32{}; }

} // namespace

bool platform_taken(const StageSections& sections, EntId self, Vec2 at, const PlatformWorld& p) {
    character::SceneHit hit;
    if (character::cast_nearest(p.scene, p.hull, at, Vec2{}, hit)) return true;
    const Vec2 half = hull_half(p);
    for (uint32_t i = 0; i < sections.count; ++i) {
        const character::Character& o = sections.at[i].c;
        if (!(sections.at[i].id == self) && near(o.position.x, at.x, half.x + half.x) &&
            near(o.position.y, at.y, half.y + half.y))
            return true;
    }
    return false;
}

bool belt_taken(const BodyPool& pool, const StageSections& sections, const Body& self, fix32 z, const BrawlWorld& w) {
    if (blocked(*w.floor, self.pos.x, z)) return true;
    const fix32 reach = hull_half(*w.platform).x * fix32::from_int(2);
    for (uint32_t i = 0; i < pool.count; ++i) {
        const Body& t = pool.bodies[i];
        if (t.id == self.id || sections.find(t.id) != nullptr) continue;
        if (near(t.pos.x, self.pos.x, reach) && depths_cross(z, depth_of(self, w), t.pos.z, depth_of(t, w))) return true;
    }
    return false;
}

} // namespace framework::brawl
