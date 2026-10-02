#include "tmj_parts.hpp"

namespace framework::tiled {
namespace {

bool refused_shape(const Doc& d, const json::Node& o, const std::string& who, std::string& error) {
    static const char* const KEYS[][2] = {
        {"ellipse", "is an ellipse; use a rectangle or a polygon"},
        {"polyline", "is a polyline; close it into a polygon"},
        {"text", "is a text object; the engine bakes no text objects"},
        {"gid", "is a tile object; place the tile on a tile layer"},
        {"template", "comes from a template; detach it (Detach Template) so the map holds its fields"},
    };
    for (const auto& [key, why] : KEYS)
        if (d.field(o, key) != nullptr) return d.fail(o, who + " " + why, error);
    fix32 rotation;
    if (!d.get(o, "rotation", rotation, error)) return false;
    return rotation.raw == 0 || d.fail(o, who + " is rotated; set its rotation to 0", error);
}

bool class_of(const Doc& d, const json::Node& o, const std::string& who, std::string& cls, std::string& error) {
    std::string_view type, klass;
    if (!d.get(o, "type", type, error) || !d.get(o, "class", klass, error)) return false;
    if (!type.empty() && !klass.empty() && type != klass)
        return d.fail(o, who + " has type '" + std::string(type) + "' and class '" + std::string(klass) +
                             "'; keep one", error);
    cls = type.empty() ? klass : type;
    return true;
}

bool polygon(const Doc& d, const json::Node& list, const std::string& who, tilemap::ObjectSrc& out,
             std::string& error) {
    if (list.count < 3 || list.count > tilemap::MAX_POLYGON_VERTICES)
        return d.fail(list, who + " is a polygon with " + std::to_string(list.count) + " vertices; allowed 3 to 16",
                      error);
    for (const json::Node& p : d.items(list)) {
        if (p.kind != json::Kind::Object) return d.fail(p, "a polygon point must be an object", error);
        fix32 x, y;
        if (!d.get(p, "x", x, error, Need::Required) || !d.get(p, "y", y, error, Need::Required)) return false;
        out.vertices.push_back(tilemap::ObjectVertex{x.raw, y.raw});
    }
    out.shape = tilemap::ObjectShape::Polygon;
    return true;
}

bool property(const Doc& d, const json::Node& p, const std::string& who, tilemap::ObjectPropSrc& out,
              std::string& error) {
    std::string_view name, type = "string";
    if (!d.get(p, "name", name, error, Need::Required) || !d.get(p, "type", type, error)) return false;
    out.name = name;
    const std::string what = who + " property '" + out.name + "'";
    if (type == "string") {
        std::string_view s;
        if (!d.get(p, "value", s, error, Need::Required)) return false;
        out.type = tilemap::PropType::String;
        out.text = s;
    } else if (type == "int") {
        int64_t v = 0;
        if (!d.get(p, "value", v, error, Need::Required)) return false;
        if (v < INT32_MIN || v > INT32_MAX) return d.fail(p, what + " does not fit 32 bits", error);
        out.type = tilemap::PropType::Int;
        out.value = static_cast<int32_t>(v);
    } else if (type == "float") {
        fix32 v;
        if (!d.get(p, "value", v, error, Need::Required)) return false;
        out.type = tilemap::PropType::Fix;
        out.value = v.raw;
    } else if (type == "bool") {
        bool v = false;
        if (!d.get(p, "value", v, error, Need::Required)) return false;
        out.type = tilemap::PropType::Bool;
        out.value = v ? 1 : 0;
    } else {
        return d.fail(p, what + " has type " + std::string(type) + "; allowed string, int, float, bool", error);
    }
    return true;
}

bool object(const Doc& d, const json::Node& o, const Placement& at, tilemap::ObjectSrc& out, std::string& error) {
    int64_t id = 0;
    std::string_view name;
    if (!d.get(o, "id", id, error, Need::Required) || !d.get(o, "name", name, error)) return false;
    if (id < 0 || id > INT32_MAX) return d.fail(o, "object id " + std::to_string(id) + " is out of range", error);
    out.id = static_cast<uint32_t>(id);
    out.name = name;
    const std::string who = "object '" + out.name + "' (id " + std::to_string(id) + ")";
    bool point = false;
    const json::Node* poly = nullptr;
    fix32 x, y;
    if (!refused_shape(d, o, who, error) || !class_of(d, o, who, out.cls, error) ||
        !d.get(o, "point", point, error) || !d.expect(o, "polygon", json::Kind::Array, Need::Optional, poly, error) ||
        !d.get(o, "x", x, error, Need::Required) || !d.get(o, "y", y, error, Need::Required))
        return false;
    out.x = at.offset_x + x;
    out.y = at.offset_y + y;
    if (poly != nullptr) {
        if (!polygon(d, *poly, who, out, error)) return false;
    } else if (!point) {
        out.shape = tilemap::ObjectShape::Rect;
        if (!d.get(o, "width", out.w, error) || !d.get(o, "height", out.h, error)) return false;
        if (out.w.raw < 0 || out.h.raw < 0) return d.fail(o, who + " has a negative size", error);
    }
    const json::Node* props = nullptr;
    if (!d.expect(o, "properties", json::Kind::Array, Need::Optional, props, error)) return false;
    if (props == nullptr) return true;
    for (const json::Node& p : d.items(*props)) {
        if (p.kind != json::Kind::Object) return d.fail(p, "a property must be an object", error);
        tilemap::ObjectPropSrc prop;
        if (!property(d, p, who, prop, error)) return false;
        for (const tilemap::ObjectPropSrc& q : out.props)
            if (q.name == prop.name) return d.fail(p, who + " has property '" + prop.name + "' twice", error);
        out.props.push_back(std::move(prop));
    }
    return true;
}

} // namespace

bool read_objects(const Doc& d, const json::Node& group, const Placement& at, tilemap::ObjectMapSrc& out,
                  std::string& error) {
    const json::Node* list = nullptr;
    if (!d.expect(group, "objects", json::Kind::Array, Need::Required, list, error)) return false;
    for (const json::Node& o : d.items(*list)) {
        if (o.kind != json::Kind::Object) return d.fail(o, "an object must be a JSON object", error);
        tilemap::ObjectSrc obj;
        if (!object(d, o, at, obj, error)) return false;
        for (const tilemap::ObjectSrc& seen : out.objects)
            if (seen.id == obj.id) return d.fail(o, "object id " + std::to_string(obj.id) + " is used twice", error);
        out.objects.push_back(std::move(obj));
    }
    return true;
}

} // namespace framework::tiled
