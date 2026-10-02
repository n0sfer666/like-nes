#include "tmj_parts.hpp"

namespace framework::tiled {
namespace {

constexpr uint32_t GID_FLIP_H = 0x80000000u;
constexpr uint32_t GID_FLIP_V = 0x40000000u;
constexpr uint32_t GID_FLIP_D = 0x20000000u;
constexpr uint32_t GID_HEX_120 = 0x10000000u;
constexpr uint32_t GID_ID = 0x0FFFFFFFu;

} // namespace

bool decode_gid(const Map& map, const json::Node& cell, Gid& out, const Tileset*& ts, std::string& error) {
    const Doc& d = map.doc;
    ts = nullptr;
    if (cell.kind != json::Kind::Int || cell.i < 0 || cell.i > 0xFFFFFFFFll)
        return d.fail(cell, "a tile cell must be a gid from 0 to 4294967295", error);
    const auto raw = static_cast<uint32_t>(cell.i);
    if ((raw & GID_HEX_120) != 0)
        return d.fail(cell, "gid " + std::to_string(raw) + " carries the hexagonal rotation bit 28; "
                            "orthogonal maps do not use it", error);
    out = Gid{raw & GID_ID, (raw & GID_FLIP_H) != 0, (raw & GID_FLIP_V) != 0, (raw & GID_FLIP_D) != 0};
    if (out.id == 0) {
        out = Gid{};
        return true;
    }
    for (const Tileset& t : map.tilesets)
        if (out.id >= t.firstgid && out.id - t.firstgid < t.count) ts = &t;
    if (ts == nullptr)
        return d.fail(cell, "gid " + std::to_string(out.id) + " belongs to no tileset of the map", error);
    return true;
}

} // namespace framework::tiled
