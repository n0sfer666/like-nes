#include "object_bake.hpp"

#include <algorithm>

#include "object_read.hpp"
#include "section_bake.hpp"

namespace framework::tilemap {
namespace {

bool refuse(std::string& error, const std::string& message) {
    error = message;
    return false;
}

bool check_map(const ObjectMapSrc& m, std::string& error) {
    for (std::size_t i = 0; i < m.objects.size(); ++i) {
        const ObjectSrc& o = m.objects[i];
        const std::string who = "object " + std::to_string(o.id) + " of '" + m.name + "'";
        for (std::size_t j = 0; j < i; ++j)
            if (m.objects[j].id == o.id) return refuse(error, who + " has a duplicate id");
        if (o.shape == ObjectShape::Polygon &&
            (o.vertices.size() < 3 || o.vertices.size() > MAX_POLYGON_VERTICES))
            return refuse(error, who + " is a polygon with " + std::to_string(o.vertices.size()) +
                                     " vertices; allowed 3 to 16");
        if (o.shape != ObjectShape::Polygon && !o.vertices.empty())
            return refuse(error, who + " has vertices but is not a polygon");
        for (std::size_t p = 0; p < o.props.size(); ++p)
            for (std::size_t q = 0; q < p; ++q)
                if (o.props[q].name == o.props[p].name)
                    return refuse(error, who + " has property '" + o.props[p].name + "' twice");
    }
    return true;
}

bool check(std::span<const ObjectMapSrc> maps, std::string& error) {
    if (maps.empty()) return refuse(error, "no object map to bake");
    for (std::size_t i = 0; i < maps.size(); ++i) {
        if (maps[i].name.empty()) return refuse(error, "an object map needs a name");
        for (std::size_t j = 0; j < i; ++j)
            if (maps[j].name == maps[i].name)
                return refuse(error, "object map '" + maps[i].name + "' is declared twice");
        if (!check_map(maps[i], error)) return false;
    }
    return true;
}

MapObject object_of(core::SectionBuilder& b, const ObjectSrc& o, std::vector<ObjectVertex>& vertices,
                    std::vector<ObjectProp>& props) {
    MapObject out{};
    out.id = o.id;
    out.name_offset = b.text(o.name);
    out.class_offset = b.text(o.cls);
    out.shape = static_cast<uint8_t>(o.shape);
    out.vertex_count = static_cast<uint8_t>(o.vertices.size());
    out.x_raw = o.x.raw;
    out.y_raw = o.y.raw;
    out.w_raw = o.w.raw;
    out.h_raw = o.h.raw;
    out.first_vertex = static_cast<uint32_t>(vertices.size());
    vertices.insert(vertices.end(), o.vertices.begin(), o.vertices.end());
    out.first_prop = static_cast<uint32_t>(props.size());
    out.prop_count = static_cast<uint32_t>(o.props.size());
    for (const ObjectPropSrc& p : o.props) {
        ObjectProp row{};
        row.name_offset = b.text(p.name);
        row.type = static_cast<uint8_t>(p.type);
        row.value = p.type == PropType::String ? static_cast<int32_t>(b.text(p.text)) : p.value;
        props.push_back(row);
    }
    return out;
}

void map_of(core::SectionBuilder& b, const ObjectMapSrc& m, ObjectRow& row) {
    std::vector<const ObjectSrc*> order;
    for (const ObjectSrc& o : m.objects) order.push_back(&o);
    std::sort(order.begin(), order.end(), [](const ObjectSrc* x, const ObjectSrc* y) {
        return x->cls < y->cls || (x->cls == y->cls && x->id < y->id);
    });
    std::vector<MapObject> objects;
    std::vector<ObjectVertex> vertices;
    std::vector<ObjectProp> props;
    for (const ObjectSrc* o : order) objects.push_back(object_of(b, *o, vertices, props));
    row.name_offset = b.text(m.name);
    row.object_offset = b.block(objects.data(), objects.size() * sizeof(MapObject), alignof(MapObject));
    row.object_count = static_cast<uint32_t>(objects.size());
    row.vertex_offset = b.block(vertices.data(), vertices.size() * sizeof(ObjectVertex), alignof(ObjectVertex));
    row.vertex_count = static_cast<uint32_t>(vertices.size());
    row.prop_offset = b.block(props.data(), props.size() * sizeof(ObjectProp), alignof(ObjectProp));
    row.prop_count = static_cast<uint32_t>(props.size());
}

} // namespace

bool bake_objects(std::span<const ObjectMapSrc> maps, std::vector<uint8_t>& out, std::string& error) {
    if (!check(maps, error)) return false;
    std::vector<ObjectRow> rows(maps.size(), ObjectRow{});
    core::SectionBuilder b(rows.size() * sizeof(ObjectRow));
    for (std::size_t i = 0; i < maps.size(); ++i) map_of(b, maps[i], rows[i]);
    if (!b.finish(OBJECT_MAGIC, OBJECT_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), out, error))
        return false;
    ObjectTable probe;
    if (!probe.open(out.data(), out.size()))
        return refuse(error, "baked object table fails its own reader (negative size or point with size)");
    return true;
}

} // namespace framework::tilemap
