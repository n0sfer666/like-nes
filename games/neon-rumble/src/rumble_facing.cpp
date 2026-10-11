#include "rumble_facing.hpp"

#include <cstring>

namespace rumble {

namespace tl = framework::tilemap;

int8_t facing_of(const tl::ObjectMap& map, const tl::MapObject& o) {
    const tl::ObjectProp* p = tl::object_prop(map, o, "facing");
    const bool left = p != nullptr && p->type == static_cast<uint8_t>(tl::PropType::String) &&
                      std::strcmp(tl::object_text(map, static_cast<uint32_t>(p->value)), "left") == 0;
    return left ? int8_t{-1} : int8_t{1};
}

} // namespace rumble
