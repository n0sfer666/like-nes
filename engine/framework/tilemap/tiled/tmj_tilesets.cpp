#include "tmj_parts.hpp"
#include "tmj_tiles.hpp"
#include "visual_format.hpp"

namespace framework::tiled {
namespace {

constexpr int64_t MAX_GRID = 65535;

struct Grid {
    int64_t tile = 0, columns = 0, count = 0, margin = 0, spacing = 0, image_w = 0, image_h = 0;
};

bool fits(const Grid& g, const Image& img) {
    const int64_t rows = (g.count + g.columns - 1) / g.columns;
    const auto span = [&](int64_t n) { return g.margin + n * g.tile + (n - 1) * g.spacing; };
    return span(g.columns) <= img.width && span(rows) <= img.height;
}

bool image_of(Map& map, const Doc& d, const json::Node& ts, const std::string& who, Grid& g, Image& img,
              std::string& error) {
    const json::Node* image = nullptr;
    if (!d.expect(ts, "image", json::Kind::String, Need::Optional, image, error)) return false;
    if (image == nullptr || image->s.empty())
        return d.fail(ts, who + " is a collection of images; build it from one image "
                                "(New Tileset, Based on Tileset Image)", error);
    if (d.field(ts, "transparentcolor") != nullptr)
        return d.fail(ts, who + " uses a transparent color; put transparency into the PNG alpha", error);
    if (!d.get(ts, "imagewidth", g.image_w, error, Need::Required) ||
        !d.get(ts, "imageheight", g.image_h, error, Need::Required))
        return false;
    std::string why;
    if (!map.src.image(d.file, std::string(image->s), img, why)) return d.fail(*image, why, error);
    if (img.width != g.image_w || img.height != g.image_h)
        return d.fail(*image, who + ": '" + std::string(image->s) + "' is " + std::to_string(img.width) + "x" +
                                  std::to_string(img.height) + " on disk, the tileset says " +
                                  std::to_string(g.image_w) + "x" + std::to_string(g.image_h) +
                                  "; reload the image in Tiled", error);
    if (!fits(g, img)) return d.fail(ts, who + ": the tile grid does not fit its image", error);
    return true;
}

bool offset_free(const Doc& d, const json::Node& ts, const std::string& who, std::string& error) {
    const json::Node* off = nullptr;
    if (!d.expect(ts, "tileoffset", json::Kind::Object, Need::Optional, off, error)) return false;
    if (off == nullptr) return true;
    int64_t x = 0, y = 0;
    if (!d.get(*off, "x", x, error) || !d.get(*off, "y", y, error)) return false;
    return (x == 0 && y == 0) || d.fail(*off, who + " has a drawing offset; set Tile Offset to 0,0", error);
}

bool grid_of(const Map& map, const Doc& d, const json::Node& ts, const std::string& who, Grid& g,
             std::string& error) {
    int64_t height = 0;
    if (!d.get(ts, "tilewidth", g.tile, error, Need::Required) ||
        !d.get(ts, "tileheight", height, error, Need::Required) ||
        !d.get(ts, "columns", g.columns, error, Need::Required) ||
        !d.get(ts, "tilecount", g.count, error, Need::Required) ||
        !d.get(ts, "margin", g.margin, error) || !d.get(ts, "spacing", g.spacing, error))
        return false;
    if (g.tile != height)
        return d.fail(ts, who + " has non-square tiles " + std::to_string(g.tile) + "x" + std::to_string(height) +
                              "; the engine draws square tiles", error);
    if (g.tile != map.tile)
        return d.fail(ts, who + " has " + std::to_string(g.tile) + " px tiles, the map uses " +
                              std::to_string(map.tile) + " px; one tile size per map", error);
    if (g.count < 1 || g.count > tilemap::MAX_VISUAL_TILES)
        return d.fail(ts, who + " has " + std::to_string(g.count) + " tiles; a map holds 1 to 8191 tiles "
                                "over all its tilesets", error);
    if (g.columns < 1 || g.columns > MAX_GRID || g.margin < 0 || g.margin > MAX_GRID || g.spacing < 0 ||
        g.spacing > MAX_GRID)
        return d.fail(ts, who + " has columns, margin or spacing out of range", error);
    return true;
}

bool read_tileset(Map& map, const Doc& d, const json::Node& ts, Tileset& out, Level& level, std::string& error) {
    std::string_view name;
    if (!d.get(ts, "name", name, error)) return false;
    out.name = name;
    const std::string who = "tileset '" + out.name + "'";
    Grid g;
    Image img;
    if (!grid_of(map, d, ts, who, g, error) || !offset_free(d, ts, who, error) ||
        !image_of(map, d, ts, who, g, img, error))
        return false;
    out.count = static_cast<uint32_t>(g.count);
    out.flags.assign(out.count, std::nullopt);
    level.visual.tilesets.push_back(tilemap::VisualTileset{img.guid, out.first_index, out.count,
                                                           static_cast<uint32_t>(g.columns),
                                                           static_cast<uint32_t>(g.margin),
                                                           static_cast<uint32_t>(g.spacing), 0});
    return read_tiles(d, ts, out, level.visual.anims, error);
}

bool external(Map& map, const json::Node& entry, std::string_view source, Tileset& ts, Level& level,
              std::string& error) {
    const Doc& d = map.doc;
    if (source.size() >= 4 && source.substr(source.size() - 4) == ".tsx")
        return d.fail(entry, "tileset '" + std::string(source) + "' is XML; export it as JSON (.tsj) "
                                 "and point the map at the .tsj", error);
    Doc td;
    std::vector<uint8_t> bytes;
    std::string why;
    if (!map.src.tileset(d.file, std::string(source), td.file, bytes, why)) return d.fail(entry, why, error);
    td.in = std::as_bytes(std::span<const uint8_t>(bytes));
    return td.load(error) && read_tileset(map, td, td.root(), ts, level, error);
}

} // namespace

bool load_tilesets(Map& map, Level& out, std::string& error) {
    const Doc& d = map.doc;
    const json::Node* list = nullptr;
    if (!d.expect(d.root(), "tilesets", json::Kind::Array, Need::Required, list, error)) return false;
    uint32_t used = 0;
    int64_t next_gid = 1;
    for (const json::Node& entry : d.items(*list)) {
        if (entry.kind != json::Kind::Object) return d.fail(entry, "a tileset entry must be an object", error);
        int64_t firstgid = 0;
        std::string_view source;
        if (!d.get(entry, "firstgid", firstgid, error, Need::Required) || !d.get(entry, "source", source, error))
            return false;
        if (firstgid < next_gid)
            return d.fail(entry, "tileset firstgid " + std::to_string(firstgid) + " overlaps the previous tileset",
                          error);
        if (firstgid > 0x0FFFFFFF)
            return d.fail(entry, "tileset firstgid " + std::to_string(firstgid) + " does not fit 28 bits", error);
        Tileset ts;
        ts.firstgid = static_cast<uint32_t>(firstgid);
        ts.first_index = used + 1;
        if (!(source.empty() ? read_tileset(map, d, entry, ts, out, error)
                             : external(map, entry, source, ts, out, error)))
            return false;
        if (ts.count > tilemap::MAX_VISUAL_TILES - used)
            return d.fail(entry, "the map uses more than 8191 tiles over its tilesets", error);
        used += ts.count;
        next_gid = firstgid + ts.count;
        map.tilesets.push_back(std::move(ts));
    }
    return true;
}

} // namespace framework::tiled
