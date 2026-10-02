#include "tmj_parts.hpp"

namespace framework::tiled {
namespace {

constexpr tilemap::TileFlags SYMMETRIC = tilemap::TILE_SOLID | tilemap::TILE_ONEWAY;

} // namespace

bool bake_collision(const Map& map, const json::Node& data, const Placement& at, const std::string& layer_name,
                    tilemap::ParsedMap& out, std::string& error) {
    const Doc& d = map.doc;
    out.origin = Vec2{at.offset_x, at.offset_y};
    out.tile_size = fix32::from_int(static_cast<int32_t>(map.tile));
    out.width = map.width;
    out.height = map.height;
    out.flags.clear();
    out.flags.reserve(data.count);
    for (const json::Node& cell : d.items(data)) {
        Gid g;
        const Tileset* ts = nullptr;
        if (!decode_gid(map, cell, g, ts, error)) return false;
        if (ts == nullptr) {
            out.flags.push_back(tilemap::TILE_EMPTY);
            continue;
        }
        const uint32_t local = g.id - ts->firstgid;
        const auto x = out.flags.size() % map.width, y = out.flags.size() / map.width;
        const std::string where = " at cell " + std::to_string(x) + "," + std::to_string(y) + " of collision layer '" +
                                  layer_name + "'";
        const std::optional<tilemap::TileFlags> flags = ts->flags[local];
        if (!flags)
            return d.fail(cell, "tile " + std::to_string(local) + " of tileset '" + ts->name +
                                    "' has no 'flags' property" + where, error);
        tilemap::TileFlags bits = *flags;
        if ((bits & tilemap::TILE_SLOPE) != 0) {
            if (g.flip_v || g.flip_d)
                return d.fail(cell, "slope tile " + std::to_string(local) + " is flipped vertically or rotated" +
                                        where + "; only a horizontal flip mirrors a slope", error);
            if (g.flip_h) bits ^= tilemap::TILE_SLOPE_FLIP_X;
        } else if ((g.flip_h || g.flip_v || g.flip_d) && (bits & ~SYMMETRIC) != 0) {
            return d.fail(cell, "tile " + std::to_string(local) + " is flipped or rotated" + where +
                                    "; only empty, solid and one-way tiles ignore flips and a slope mirrors "
                                    "horizontally, use an unflipped tile", error);
        }
        out.flags.push_back(bits);
    }
    return true;
}

} // namespace framework::tiled
