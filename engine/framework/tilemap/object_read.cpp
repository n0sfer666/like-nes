#include "object_read.hpp"

#include <algorithm>
#include <cstring>

namespace framework::tilemap {
namespace {

bool prop_ok(const core::SectionView& v, const ObjectProp& p) {
    if (!v.text(p.name_offset) || p.type > static_cast<uint8_t>(PropType::Bool)) return false;
    if (p.type == static_cast<uint8_t>(PropType::String))
        return p.value >= 0 && v.text(static_cast<uint32_t>(p.value));
    if (p.type == static_cast<uint8_t>(PropType::Bool)) return p.value == 0 || p.value == 1;
    return true;
}

bool shape_ok(const MapObject& o) {
    if (o.w_raw < 0 || o.h_raw < 0) return false;
    switch (o.shape) {
    case static_cast<uint8_t>(ObjectShape::Point): return o.vertex_count == 0 && o.w_raw == 0 && o.h_raw == 0;
    case static_cast<uint8_t>(ObjectShape::Rect): return o.vertex_count == 0;
    case static_cast<uint8_t>(ObjectShape::Polygon):
        return o.vertex_count >= 3 && o.vertex_count <= MAX_POLYGON_VERTICES;
    default: return false;
    }
}

bool ordered(const core::SectionView& v, const MapObject& a, const MapObject& b) {
    const int c = std::strcmp(v.strings + a.class_offset, v.strings + b.class_offset);
    return c < 0 || (c == 0 && a.id < b.id);
}

bool row_ok(const core::SectionView& v, const ObjectRow& r) {
    std::span<const MapObject> objects;
    std::span<const ObjectVertex> vertices;
    std::span<const ObjectProp> props;
    if (!v.text(r.name_offset) || !v.take(r.object_offset, r.object_count, objects) ||
        !v.take(r.vertex_offset, r.vertex_count, vertices) || !v.take(r.prop_offset, r.prop_count, props))
        return false;
    for (const ObjectProp& p : props)
        if (!prop_ok(v, p)) return false;
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const MapObject& o = objects[i];
        if (!v.text(o.name_offset) || !v.text(o.class_offset) || !shape_ok(o)) return false;
        if (uint64_t{o.first_vertex} + o.vertex_count > vertices.size()) return false;
        if (uint64_t{o.first_prop} + o.prop_count > props.size()) return false;
        if (i > 0 && !ordered(v, objects[i - 1], o)) return false;
    }
    return true;
}

} // namespace

bool ObjectTable::open(const void* data, std::size_t size) {
    view_ = core::SectionView{};
    rows_ = nullptr;
    core::SectionView v;
    if (!core::open_section(data, size, OBJECT_MAGIC, OBJECT_VERSION, sizeof(ObjectRow), alignof(ObjectRow), v))
        return false;
    const std::span<const ObjectRow> rows = v.at<ObjectRow>(v.header->rows_offset, v.header->count);
    for (const ObjectRow& r : rows)
        if (!row_ok(v, r)) return false;
    view_ = v;
    rows_ = rows.data();
    return true;
}

const char* ObjectTable::name(uint32_t index) const {
    if (index >= count()) return "";
    return view_.strings + rows_[index].name_offset;
}

ObjectMap ObjectTable::map(uint32_t index) const {
    if (index >= count()) return {};
    const ObjectRow& r = rows_[index];
    ObjectMap m;
    m.row = &r;
    m.objects = view_.at<MapObject>(r.object_offset, r.object_count);
    m.vertices = view_.at<ObjectVertex>(r.vertex_offset, r.vertex_count);
    m.props = view_.at<ObjectProp>(r.prop_offset, r.prop_count);
    m.strings = view_.strings;
    return m;
}

ObjectMap ObjectTable::find(const char* wanted) const {
    if (wanted == nullptr) return {};
    for (uint32_t i = 0; i < count(); ++i)
        if (std::strcmp(name(i), wanted) == 0) return map(i);
    return {};
}

const char* object_text(const ObjectMap& map, uint32_t offset) {
    return map.strings == nullptr ? "" : map.strings + offset;
}

std::span<const MapObject> objects_of_class(const ObjectMap& map, std::string_view cls) {
    const auto below = [&](const MapObject& o, std::string_view c) { return object_text(map, o.class_offset) < c; };
    const auto above = [&](std::string_view c, const MapObject& o) { return c < object_text(map, o.class_offset); };
    const auto first = std::lower_bound(map.objects.begin(), map.objects.end(), cls, below);
    const auto last = std::upper_bound(first, map.objects.end(), cls, above);
    return {first, last};
}

std::span<const ObjectVertex> object_vertices(const ObjectMap& map, const MapObject& object) {
    return map.vertices.subspan(object.first_vertex, object.vertex_count);
}

std::span<const ObjectProp> object_props(const ObjectMap& map, const MapObject& object) {
    return map.props.subspan(object.first_prop, object.prop_count);
}

const ObjectProp* object_prop(const ObjectMap& map, const MapObject& object, std::string_view name) {
    for (const ObjectProp& p : object_props(map, object))
        if (name == object_text(map, p.name_offset)) return &p;
    return nullptr;
}

} // namespace framework::tilemap
