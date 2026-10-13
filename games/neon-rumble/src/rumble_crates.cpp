#include "rumble_crates.hpp"

#include <cstdio>

#include "rumble_floor.hpp"
#include "rumble_roster.hpp"

namespace rumble {

namespace tl = framework::tilemap;
namespace br = framework::brawl;

bool place_crates(const Level& level, const tl::ObjectMap& map, const br::DepthFloor& floor, br::BodyPool& pool) {
    for (const tl::MapObject& o : tl::objects_of_class(map, "crate")) {
        br::Body b;
        b.pos.x = fix32::from_raw(o.x_raw);
        b.pos.z = fix32::from_raw(o.y_raw);
        b.team = CRATE_TEAM;
        b.hp = CRATE_HP;
        b.kind = static_cast<uint8_t>(CRATE);
        const char* name = tl::object_text(map, o.name_offset);
        if (o.shape != static_cast<uint8_t>(tl::ObjectShape::Point) || !on_floor(floor, b.pos.x, b.pos.z)) {
            std::fprintf(stderr, "neon-rumble: level %s: crate %s is not a point on the floor\n", level.name, name);
            return false;
        }
        if (pool.spawn(b) == br::EntId{}) {
            std::fprintf(stderr, "neon-rumble: level %s: crate %s does not fit the body pool\n", level.name, name);
            return false;
        }
    }
    return true;
}

} // namespace rumble
