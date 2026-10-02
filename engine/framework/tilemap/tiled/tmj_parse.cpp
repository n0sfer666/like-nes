#include "tmj_parts.hpp"

namespace framework::tiled {
namespace {

bool hex_digit(char c, uint32_t& out) {
    if (c >= '0' && c <= '9') out = static_cast<uint32_t>(c - '0');
    else if (c >= 'a' && c <= 'f') out = static_cast<uint32_t>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') out = static_cast<uint32_t>(c - 'A' + 10);
    else return false;
    return true;
}

bool background(const Doc& d, uint32_t& rgba, std::string& error) {
    const json::Node* node = nullptr;
    if (!d.expect(d.root(), "backgroundcolor", json::Kind::String, Need::Optional, node, error)) return false;
    rgba = 0;
    if (node == nullptr) return true;
    std::string_view s;
    if (!d.get(d.root(), "backgroundcolor", s, error)) return false;
    const auto bad = [&] { return d.fail(*node, "backgroundcolor must be #RRGGBB or #AARRGGBB", error); };
    if ((s.size() != 7 && s.size() != 9) || s[0] != '#') return bad();
    uint32_t argb = 0;
    for (const char c : s.substr(1)) {
        uint32_t v = 0;
        if (!hex_digit(c, v)) return bad();
        argb = argb << 4 | v;
    }
    if (s.size() == 7) argb |= 0xFF000000u;
    rgba = argb << 8 | argb >> 24;
    return true;
}

bool geometry(const Doc& d, Map& map, std::string& error) {
    const json::Node& r = d.root();
    std::string_view orientation;
    bool infinite = false;
    int64_t w = 0, h = 0, tw = 0, th = 0;
    fix32 ox, oy;
    if (!d.get(r, "orientation", orientation, error, Need::Required) || !d.get(r, "infinite", infinite, error) ||
        !d.get(r, "width", w, error, Need::Required) || !d.get(r, "height", h, error, Need::Required) ||
        !d.get(r, "tilewidth", tw, error, Need::Required) || !d.get(r, "tileheight", th, error, Need::Required) ||
        !d.get(r, "parallaxoriginx", ox, error) || !d.get(r, "parallaxoriginy", oy, error))
        return false;
    const auto at = [&](std::string_view key) -> const json::Node& { return *d.field(r, key); };
    if (orientation != "orthogonal")
        return d.fail(at("orientation"),
                      "map orientation is " + std::string(orientation) + "; the engine draws orthogonal maps", error);
    if (infinite) return d.fail(at("infinite"), "the map is infinite; turn Map Properties -> Infinite off", error);
    if (tw != th) return d.fail(at("tileheight"), "map tiles are not square", error);
    if (tw < 1 || tw > 1024) return d.fail(at("tilewidth"), "map tile size must be 1 to 1024 pixels", error);
    const auto most = static_cast<int64_t>(tilemap::MAX_MAP_TILES);
    if (w < 1 || h < 1 || w > most || h > most || w * h > most)
        return d.fail(at("width"), "map size " + std::to_string(w) + "x" + std::to_string(h) + " is out of range",
                      error);
    if (ox.raw != 0 || oy.raw != 0)
        return d.fail(at(ox.raw != 0 ? "parallaxoriginx" : "parallaxoriginy"),
                      "the map sets a parallax origin; keep Parallax Origin at 0,0", error);
    map.width = static_cast<uint32_t>(w);
    map.height = static_cast<uint32_t>(h);
    map.tile = static_cast<uint32_t>(tw);
    return true;
}

} // namespace

bool import_tmj(const std::string& name, const std::string& file, std::span<const std::byte> bytes, Source& src,
                Level& out, std::string& error) {
    Doc d;
    d.file = file;
    d.in = bytes;
    if (!d.load(error)) return false;
    Map map{d, src, 0, 0, 0, {}};
    out = Level{};
    if (!geometry(d, map, error) || !background(d, out.visual.background_rgba, error) ||
        !load_tilesets(map, out, error) || !walk_layers(map, out, error))
        return false;
    out.collision.name = name;
    out.visual.name = name;
    out.visual.width = map.width;
    out.visual.height = map.height;
    out.visual.tile_size = map.tile;
    out.objects.name = name;
    return true;
}

} // namespace framework::tiled
