#include "tmj_parts.hpp"

namespace framework::tiled {
namespace {

constexpr fix32 UNIT = fix32::from_int(1);

struct Walk {
    Map& map;
    Level& out;
    bool collision = false;
};

bool combine(int64_t raw, fix32& out) {
    if (raw < INT32_MIN || raw > INT32_MAX) return false;
    out = fix32::from_raw(static_cast<int32_t>(raw));
    return true;
}

bool placement(const Doc& d, const json::Node& layer, const std::string& who, const Placement& parent,
               Placement& at, std::string& error) {
    fix32 ox, oy, px = UNIT, py = UNIT, opacity = UNIT;
    bool visible = true;
    if (!d.get(layer, "offsetx", ox, error) || !d.get(layer, "offsety", oy, error) ||
        !d.get(layer, "parallaxx", px, error) || !d.get(layer, "parallaxy", py, error) ||
        !d.get(layer, "opacity", opacity, error) || !d.get(layer, "visible", visible, error))
        return false;
    if (d.field(layer, "tintcolor") != nullptr)
        return d.fail(layer, who + " uses a tint color; remove Tint Color and tint the PNG instead", error);
    if (opacity.raw < 0 || opacity.raw > fix32::ONE) return d.fail(layer, who + " has opacity outside 0..1", error);
    if (!combine(int64_t{parent.offset_x.raw} + ox.raw, at.offset_x) ||
        !combine(int64_t{parent.offset_y.raw} + oy.raw, at.offset_y) ||
        !combine(fix32::shift_down(int64_t{parent.parallax_x.raw} * px.raw), at.parallax_x) ||
        !combine(fix32::shift_down(int64_t{parent.parallax_y.raw} * py.raw), at.parallax_y))
        return d.fail(layer, who + " with its groups adds up to an offset or parallax past +-32767; "
                             "move the offset into fewer groups", error);
    at.opacity = parent.opacity * opacity;
    at.visible = parent.visible && visible;
    return true;
}

bool prop_flag(const Doc& d, const json::Node& layer, std::string_view name, bool& out, std::string& error) {
    const json::Node* v = nullptr;
    if (!d.prop(layer, name, "bool", v, error)) return false;
    out = v != nullptr && v->i != 0;
    return true;
}

tilemap::VisualLayerSrc visual_of(std::string_view name, const Placement& at, tilemap::LayerKind kind) {
    tilemap::VisualLayerSrc v;
    v.name = name;
    v.kind = kind;
    v.opacity = static_cast<uint8_t>((int64_t{at.opacity.raw} * 255 + fix32::ONE / 2) >> fix32::SHIFT);
    v.parallax_x = at.parallax_x;
    v.parallax_y = at.parallax_y;
    v.offset_x = at.offset_x;
    v.offset_y = at.offset_y;
    return v;
}

bool layer_data(const Map& map, const json::Node& layer, const std::string& who, const json::Node*& data,
                std::string& error) {
    const Doc& d = map.doc;
    std::string_view encoding = "csv", compression;
    int64_t w = 0, h = 0;
    if (!d.get(layer, "encoding", encoding, error) || !d.get(layer, "compression", compression, error)) return false;
    if (encoding != "csv" || !compression.empty())
        return d.fail(layer, who + " is stored " + std::string(encoding) +
                                 "; set Tile Layer Format to CSV in Map Properties", error);
    if (!d.get(layer, "width", w, error, Need::Required) || !d.get(layer, "height", h, error, Need::Required) ||
        !d.expect(layer, "data", json::Kind::Array, Need::Required, data, error))
        return false;
    if (w != map.width || h != map.height || data->count != uint64_t{map.width} * map.height)
        return d.fail(layer, who + " does not match the map size", error);
    return true;
}

bool tile_layer(Walk& w, const json::Node& layer, std::string_view name, const Placement& at, std::string& error) {
    const Doc& d = w.map.doc;
    const std::string who = "tile layer '" + std::string(name) + "'";
    bool collision = false, visible_too = false, rx = false, ry = false;
    const json::Node* data = nullptr;
    if (!prop_flag(d, layer, "collision", collision, error) ||
        !prop_flag(d, layer, "visible_too", visible_too, error) ||
        !prop_flag(d, layer, "repeat_x", rx, error) || !prop_flag(d, layer, "repeat_y", ry, error) ||
        !layer_data(w.map, layer, who, data, error))
        return false;
    if (collision) {
        if (w.collision)
            return d.fail(layer, who + " is a second collision layer; exactly one tile layer has collision = true",
                          error);
        if (at.parallax_x != UNIT || at.parallax_y != UNIT)
            return d.fail(layer, who + " is the collision layer and has parallax; keep parallax 1", error);
        w.collision = true;
        if (!bake_collision(w.map, *data, at, std::string(name), w.out.collision, error)) return false;
        if (!visible_too) return true;
    }
    if (!at.visible) return true;
    tilemap::VisualLayerSrc v = visual_of(name, at, tilemap::LayerKind::Tile);
    v.repeat = static_cast<uint8_t>((rx ? tilemap::REPEAT_X : 0) | (ry ? tilemap::REPEAT_Y : 0));
    for (const json::Node& cell : d.items(*data)) {
        Gid g;
        const Tileset* ts = nullptr;
        if (!decode_gid(w.map, cell, g, ts, error)) return false;
        const uint32_t index = ts == nullptr ? 0 : ts->first_index + (g.id - ts->firstgid);
        v.cells.push_back(static_cast<uint16_t>(index | (g.flip_d ? tilemap::CELL_FLIP_D : 0) |
                                                (g.flip_v ? tilemap::CELL_FLIP_V : 0) |
                                                (g.flip_h ? tilemap::CELL_FLIP_H : 0)));
    }
    w.out.visual.layers.push_back(std::move(v));
    return true;
}

bool image_layer(Walk& w, const json::Node& layer, std::string_view name, const Placement& at, std::string& error) {
    const Doc& d = w.map.doc;
    const std::string who = "image layer '" + std::string(name) + "'";
    if (d.field(layer, "transparentcolor") != nullptr)
        return d.fail(layer, who + " uses a transparent color; put transparency into the PNG alpha", error);
    const json::Node* image = nullptr;
    bool rx = false, ry = false;
    if (!d.expect(layer, "image", json::Kind::String, Need::Required, image, error) ||
        !d.get(layer, "repeatx", rx, error) || !d.get(layer, "repeaty", ry, error))
        return false;
    if (image->s.empty()) return d.fail(layer, who + " has no image", error);
    if (!at.visible) return true;
    Image img;
    std::string why;
    if (!w.map.src.image(d.file, std::string(image->s), img, why)) return d.fail(*image, why, error);
    tilemap::VisualLayerSrc v = visual_of(name, at, tilemap::LayerKind::Image);
    v.repeat = static_cast<uint8_t>((rx ? tilemap::REPEAT_X : 0) | (ry ? tilemap::REPEAT_Y : 0));
    v.image_guid = img.guid;
    v.image_w = img.width;
    v.image_h = img.height;
    w.out.visual.layers.push_back(std::move(v));
    return true;
}

bool object_layer(Walk& w, const json::Node& layer, std::string_view name, const Placement& at, std::string& error) {
    const Doc& d = w.map.doc;
    if (at.parallax_x != UNIT || at.parallax_y != UNIT)
        return d.fail(layer, "object layer '" + std::string(name) + "' has parallax; objects live in the world, "
                             "keep parallax 1", error);
    return read_objects(d, layer, at, w.out.objects, error);
}

bool walk(Walk& w, const json::Node& list, const Placement& parent, std::string& error) {
    const Doc& d = w.map.doc;
    for (const json::Node& layer : d.items(list)) {
        if (layer.kind != json::Kind::Object) return d.fail(layer, "a layer must be an object", error);
        std::string_view type, name;
        if (!d.get(layer, "type", type, error, Need::Required) || !d.get(layer, "name", name, error)) return false;
        Placement at;
        if (!placement(d, layer, "layer '" + std::string(name) + "'", parent, at, error)) return false;
        const json::Node* children = nullptr;
        bool ok = false;
        if (type == "group")
            ok = d.expect(layer, "layers", json::Kind::Array, Need::Required, children, error) &&
                 walk(w, *children, at, error);
        else if (type == "tilelayer") ok = tile_layer(w, layer, name, at, error);
        else if (type == "imagelayer") ok = image_layer(w, layer, name, at, error);
        else if (type == "objectgroup") ok = object_layer(w, layer, name, at, error);
        else ok = d.fail(layer, "unknown layer type '" + std::string(type) + "'", error);
        if (!ok) return false;
    }
    return true;
}

} // namespace

bool walk_layers(Map& map, Level& out, std::string& error) {
    const Doc& d = map.doc;
    const json::Node* list = nullptr;
    if (!d.expect(d.root(), "layers", json::Kind::Array, Need::Required, list, error)) return false;
    Walk w{map, out};
    if (!walk(w, *list, Placement{}, error)) return false;
    if (!w.collision)
        return d.fail(d.root(), "the map has no collision layer; give one tile layer the bool property "
                                "collision = true", error);
    return true;
}

} // namespace framework::tiled
