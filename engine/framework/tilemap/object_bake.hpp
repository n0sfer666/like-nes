#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "fixed.hpp"
#include "object_format.hpp"

namespace framework::tilemap {

struct ObjectPropSrc {
    std::string name;
    PropType type = PropType::Int;
    int32_t value = 0;
    std::string text;
};

struct ObjectSrc {
    uint32_t id = 0;
    std::string name;
    std::string cls;
    ObjectShape shape = ObjectShape::Point;
    fix32 x, y, w, h;
    std::vector<ObjectVertex> vertices;
    std::vector<ObjectPropSrc> props;
};

struct ObjectMapSrc {
    std::string name;
    std::vector<ObjectSrc> objects;
};

bool bake_objects(std::span<const ObjectMapSrc> maps, std::vector<uint8_t>& out, std::string& error);

} // namespace framework::tilemap
