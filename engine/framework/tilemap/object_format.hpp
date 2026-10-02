#pragma once
#include <cstdint>

#include "section_format.hpp"

namespace framework::tilemap {

constexpr uint8_t OBJECT_MAGIC[4] = {'L', 'N', 'O', 'B'};
constexpr uint32_t OBJECT_VERSION = 1;
constexpr uint32_t MAX_POLYGON_VERTICES = 16;

enum class ObjectShape : uint8_t { Point = 0, Rect = 1, Polygon = 2 };
enum class PropType : uint8_t { String = 0, Int = 1, Fix = 2, Bool = 3 };

struct ObjectRow {
    uint32_t name_offset;
    uint32_t object_offset;
    uint32_t object_count;
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t prop_offset;
    uint32_t prop_count;
    uint32_t pad;
};
static_assert(sizeof(ObjectRow) == 32, "ObjectRow layout pinned (zero-parse ABI)");

struct MapObject {
    uint32_t id;
    uint32_t name_offset;
    uint32_t class_offset;
    uint8_t shape;
    uint8_t vertex_count;
    uint16_t pad;
    int32_t x_raw;
    int32_t y_raw;
    int32_t w_raw;
    int32_t h_raw;
    uint32_t first_vertex;
    uint32_t first_prop;
    uint32_t prop_count;
};
static_assert(sizeof(MapObject) == 44, "MapObject layout pinned (zero-parse ABI)");

struct ObjectVertex {
    int32_t x_raw;
    int32_t y_raw;
};
static_assert(sizeof(ObjectVertex) == 8, "ObjectVertex layout pinned (zero-parse ABI)");

struct ObjectProp {
    uint32_t name_offset;
    uint8_t type;
    uint8_t pad[3];
    int32_t value;
};
static_assert(sizeof(ObjectProp) == 12, "ObjectProp layout pinned (zero-parse ABI)");

} // namespace framework::tilemap
