#pragma once
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include "object_read.hpp"
#include "scene_objects.hpp"

namespace framework::scene {

class SceneObject {
public:
    SceneObject(const tilemap::ObjectMap& map, const tilemap::MapObject& object, std::string& error)
        : map_(map), object_(object), error_(error) {}

    bool shape(tilemap::ObjectShape want) const;
    bool has(std::string_view name) const { return tilemap::object_prop(map_, object_, name) != nullptr; }
    bool int_prop(std::string_view name, int32_t& out) const;
    bool optional_int(std::string_view name, int32_t& out) const;
    bool fix_prop(std::string_view name, fix32& out) const;
    bool bool_prop(std::string_view name, bool& out) const;
    bool text_prop(std::string_view name, std::string_view& out) const;
    bool word_prop(std::string_view name, std::initializer_list<std::string_view> words, uint8_t& index) const;
    bool at_least(std::string_view name, int32_t value, int32_t floor) const;
    bool fail(const std::string& why) const;
    std::string who() const;

    const tilemap::MapObject& object() const { return object_; }
    Area area() const;
    Mark mark() const;
    std::span<const tilemap::ObjectVertex> vertices() const { return tilemap::object_vertices(map_, object_); }

private:
    const tilemap::ObjectProp* typed(std::string_view name, tilemap::PropType want, bool required, bool& ok) const;

    const tilemap::ObjectMap& map_;
    const tilemap::MapObject& object_;
    std::string& error_;
};

} // namespace framework::scene
