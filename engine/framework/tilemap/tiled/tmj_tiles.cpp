#include <utility>

#include "sim_tick.hpp"
#include "tile_flag_words.hpp"
#include "tmj_tiles.hpp"

namespace framework::tiled {
namespace {

constexpr int64_t MAX_FRAME_MS = 600000;
static_assert(MAX_FRAME_MS * sim::TICK_HZ / 1000 <= UINT16_MAX, "the longest frame must fit u16 ticks");

struct Pending {
    uint32_t id = 0;
    const json::Node* at = nullptr;
    std::vector<std::pair<uint32_t, uint16_t>> frames;
};

std::vector<std::string> words_of(std::string_view s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        const std::size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t') ++i;
        if (i > start) out.emplace_back(s.substr(start, i - start));
    }
    return out;
}

bool flags_of(const Doc& d, const json::Node& tile, uint32_t id, Tileset& ts, std::string& error) {
    const json::Node* flags = nullptr;
    if (!d.prop(tile, "flags", "string", flags, error)) return false;
    if (flags == nullptr) return true;
    tilemap::TileFlags bits = 0;
    std::string why;
    if (!tilemap::parse_flag_words(words_of(flags->s), bits, why))
        return d.fail(*flags, "tileset '" + ts.name + "' tile " + std::to_string(id) + ": " + why, error);
    ts.flags[id] = bits;
    return true;
}

bool frames_of(const Doc& d, const json::Node& list, const Tileset& ts, Pending& p, std::string& error) {
    for (const json::Node& f : d.items(list)) {
        if (f.kind != json::Kind::Object) return d.fail(f, "an animation frame must be an object", error);
        int64_t tile = 0, ms = 0;
        if (!d.get(f, "tileid", tile, error, Need::Required) || !d.get(f, "duration", ms, error, Need::Required))
            return false;
        if (tile < 0 || tile >= ts.count)
            return d.fail(f, "animation frame tile " + std::to_string(tile) + " is outside tileset '" + ts.name + "'",
                          error);
        if (ms < 0 || ms > MAX_FRAME_MS)
            return d.fail(f, "frame duration " + std::to_string(ms) + " ms is out of range, use 0 to 600000", error);
        p.frames.emplace_back(static_cast<uint32_t>(tile), static_cast<uint16_t>(sim::ticks_from_ms(
                                                                static_cast<uint64_t>(ms))));
    }
    return true;
}

bool emit(const Doc& d, const std::vector<Pending>& pending, const Tileset& ts,
          std::vector<tilemap::VisualAnimSrc>& anims, std::string& error) {
    for (const Pending& p : pending) {
        tilemap::VisualAnimSrc a;
        a.index = static_cast<uint16_t>(ts.first_index + p.id);
        for (const auto& [tile, ticks] : p.frames) {
            for (const Pending& q : pending)
                if (q.id == tile && tile != p.id)
                    return d.fail(*p.at, "tile " + std::to_string(p.id) + " of tileset '" + ts.name +
                                             "' animates through tile " + std::to_string(tile) +
                                             ", which is animated itself; Tiled draws such a frame static",
                                  error);
            a.frames.push_back(tilemap::VisualFrame{static_cast<uint16_t>(ts.first_index + tile), ticks});
        }
        anims.push_back(std::move(a));
    }
    return true;
}

} // namespace

bool read_tiles(const Doc& d, const json::Node& tileset, Tileset& ts, std::vector<tilemap::VisualAnimSrc>& anims,
                std::string& error) {
    const json::Node* list = nullptr;
    if (!d.expect(tileset, "tiles", json::Kind::Array, Need::Optional, list, error)) return false;
    if (list == nullptr) return true;
    std::vector<Pending> pending;
    std::vector<bool> seen(ts.count);
    for (const json::Node& t : d.items(*list)) {
        if (t.kind != json::Kind::Object) return d.fail(t, "a tile entry must be an object", error);
        int64_t id = 0;
        if (!d.get(t, "id", id, error, Need::Required)) return false;
        if (id < 0 || id >= ts.count)
            return d.fail(t, "tile id " + std::to_string(id) + " is outside tileset '" + ts.name + "'", error);
        const auto local = static_cast<uint32_t>(id);
        if (seen[local])
            return d.fail(t, "tile id " + std::to_string(id) + " appears twice in tileset '" + ts.name + "'", error);
        seen[local] = true;
        if (!flags_of(d, t, local, ts, error)) return false;
        const json::Node* anim = nullptr;
        if (!d.expect(t, "animation", json::Kind::Array, Need::Optional, anim, error)) return false;
        if (anim == nullptr || anim->count == 0) continue;
        Pending p{local, anim, {}};
        if (!frames_of(d, *anim, ts, p, error)) return false;
        pending.push_back(std::move(p));
    }
    return emit(d, pending, ts, anims, error);
}

} // namespace framework::tiled
