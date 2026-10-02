#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "object_format.hpp"
#include "section_open.hpp"

namespace framework::tilemap {

struct ObjectMap {
    const ObjectRow* row = nullptr;
    std::span<const MapObject> objects;
    std::span<const ObjectVertex> vertices;
    std::span<const ObjectProp> props;
    const char* strings = nullptr;
};

class ObjectTable {
public:
    bool open(const void* data, std::size_t size);
    bool valid() const { return view_.header != nullptr; }
    uint32_t count() const { return valid() ? view_.header->count : 0; }
    const char* name(uint32_t index) const;
    ObjectMap map(uint32_t index) const;
    ObjectMap find(const char* name) const;

private:
    core::SectionView view_;
    const ObjectRow* rows_ = nullptr;
};

std::span<const MapObject> objects_of_class(const ObjectMap& map, std::string_view cls);
const char* object_text(const ObjectMap& map, uint32_t offset);
std::span<const ObjectVertex> object_vertices(const ObjectMap& map, const MapObject& object);
std::span<const ObjectProp> object_props(const ObjectMap& map, const MapObject& object);
const ObjectProp* object_prop(const ObjectMap& map, const MapObject& object, std::string_view name);

} // namespace framework::tilemap
