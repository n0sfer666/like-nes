#include "rumble_brawl.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "depth_step.hpp"
#include "object_read.hpp"
#include "rumble_level.hpp"

namespace rumble {

namespace {

namespace tl = framework::tilemap;
namespace br = framework::brawl;

fix32 end(int32_t at, int32_t size) { return fix32::from_raw(fix32::sat(int64_t{at} + size)); }

bool rect_of(const tl::MapObject& o, br::FloorRect& out) {
    if (o.shape != static_cast<uint8_t>(tl::ObjectShape::Rect)) return false;
    out = {fix32::from_raw(o.x_raw), fix32::from_raw(o.y_raw), end(o.x_raw, o.w_raw), end(o.y_raw, o.h_raw)};
    return true;
}

bool read_floor(const Level& level, const tl::ObjectMap& map, br::DepthFloor& floor) {
    const auto bands = tl::objects_of_class(map, "depth_band");
    if (bands.size() != 1 || !rect_of(bands.front(), floor.band)) {
        std::fprintf(stderr, "neon-rumble: level %s needs one rectangle of class depth_band, has %zu\n",
                     level.name, bands.size());
        return false;
    }
    for (const tl::MapObject& o : tl::objects_of_class(map, "wall")) {
        br::FloorRect r;
        if (rect_of(o, r) && floor.add_wall(r)) continue;
        std::fprintf(stderr, "neon-rumble: level %s: wall %s is not a rectangle or does not fit\n", level.name,
                     tl::object_text(map, o.name_offset));
        return false;
    }
    return true;
}

int8_t facing_of(const tl::ObjectMap& map, const tl::MapObject& o) {
    const tl::ObjectProp* p = tl::object_prop(map, o, "facing");
    const bool left = p != nullptr && p->type == static_cast<uint8_t>(tl::PropType::String) &&
                      std::strcmp(tl::object_text(map, static_cast<uint32_t>(p->value)), "left") == 0;
    return left ? int8_t{-1} : int8_t{1};
}

bool inside(const br::FloorRect& r, fix32 x, fix32 z) { return !(x < r.x0) && !(r.x1 < x) && !(z < r.z0) && !(r.z1 < z); }

bool in_wall(const br::FloorRect& r, fix32 x, fix32 z) { return r.x0 < x && x < r.x1 && r.z0 < z && z < r.z1; }

bool on_floor(const br::DepthFloor& floor, fix32 x, fix32 z) {
    if (!inside(floor.band, x, z)) return false;
    for (uint32_t i = 0; i < floor.wall_count; ++i)
        if (in_wall(floor.walls[i], x, z)) return false;
    return true;
}

bool spawn_roster(const Level& level, const tl::ObjectMap& map, const br::DepthFloor& floor, br::BodyPool& pool) {
    const auto spawns = tl::objects_of_class(map, "spawn");
    for (const RosterEntry& r : ROSTER) {
        const auto it = std::find_if(spawns.begin(), spawns.end(), [&](const tl::MapObject& o) {
            return std::strcmp(tl::object_text(map, o.name_offset), r.spawn) == 0;
        });
        if (it == spawns.end()) {
            std::fprintf(stderr, "neon-rumble: level %s has no spawn named %s for %s\n", level.name, r.spawn,
                         r.fighter);
            return false;
        }
        br::Body b;
        b.pos.x = fix32::from_raw(it->x_raw);
        b.pos.z = fix32::from_raw(it->y_raw);
        b.facing = facing_of(map, *it);
        b.hp = 100;
        if (!on_floor(floor, b.pos.x, b.pos.z)) {
            std::fprintf(stderr, "neon-rumble: level %s: spawn %s is outside the depth band or inside a wall\n",
                         level.name, r.spawn);
            return false;
        }
        if (pool.spawn(b) == br::EntId{}) {
            std::fprintf(stderr, "neon-rumble: body pool is full, %s is not spawned\n", r.fighter);
            return false;
        }
    }
    return true;
}

br::BrawlInput standing() {
    br::BrawlInput in;
    in.present = true;
    return in;
}

} // namespace

const br::DepthProfile Brawl::PROFILE{fix32::from_int(2), fix32::from_int(1), fix32::from_raw(fix32::ONE / 2),
                                      fix32::from_int(6)};

bool Brawl::open(const Level& level) {
    floor = br::DepthFloor{};
    pool = br::BodyPool{};
    player = br::BrawlInput{};
    tl::ObjectTable objects;
    if (!level.open_objects(objects)) return false;
    const tl::ObjectMap map = objects.find(level.name);
    return read_floor(level, map, floor) && spawn_roster(level, map, floor, pool);
}

void Brawl::step() {
    for (uint32_t i = 0; i < pool.count; ++i)
        br::step_body(pool.bodies[i], i == PLAYER ? player : standing(), PROFILE, floor);
}

DrawOrder Brawl::draw_order() const {
    DrawOrder order{};
    for (uint32_t i = 0; i < FIGHTERS; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        const br::Body& p = body(a);
        const br::Body& q = body(b);
        if (p.pos.z.raw != q.pos.z.raw) return p.pos.z.raw < q.pos.z.raw;
        return p.id.seq < q.id.seq;
    });
    return order;
}

} // namespace rumble
