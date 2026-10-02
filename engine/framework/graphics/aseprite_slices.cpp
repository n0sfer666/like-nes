#include "aseprite_parts.hpp"

namespace framework::graphics::clip_import {
namespace {

bool read_key(const tiled::Doc& d, const json::Node& k, const std::string& slice, std::size_t frames, AseKey& out,
              std::string& error) {
    if (k.kind != json::Kind::Object) return d.fail(k, "a slice key must be an object", error);
    int64_t frame = 0;
    if (!d.get(k, "frame", frame, error, tiled::Need::Required) || !read_box(d, k, "bounds", false, out.bounds, error))
        return false;
    if (frame < 0 || frame >= static_cast<int64_t>(frames))
        return d.fail(k, "slice '" + slice + "' has a key on frame " + std::to_string(frame) + " of " +
                             std::to_string(frames),
                      error);
    const Box& b = out.bounds;
    if (!box_off(b) && (b.w <= 0 || b.h <= 0 || b.w > MAX_SIDE || b.h > MAX_SIDE || b.x < 0 || b.y < 0 ||
                    b.x > MAX_SIDE || b.y > MAX_SIDE))
        return d.fail(k, "slice '" + slice + "' key on frame " + std::to_string(frame) +
                             " needs a box inside the frame; 0x0 turns the box off",
                      error);
    out.at = &k;
    out.frame = static_cast<uint32_t>(frame);
    const json::Node* pivot = d.field(k, "pivot");
    if (pivot == nullptr) return true;
    out.has_pivot = true;
    if (pivot->kind != json::Kind::Object) return d.fail(*pivot, "'pivot' must be an object", error);
    if (!d.get(*pivot, "x", out.pivot.x, error, tiled::Need::Required) ||
        !d.get(*pivot, "y", out.pivot.y, error, tiled::Need::Required))
        return false;
    if (out.pivot.x < -MAX_SIDE || out.pivot.x > MAX_SIDE || out.pivot.y < -MAX_SIDE || out.pivot.y > MAX_SIDE)
        return d.fail(*pivot, "'pivot' lies too far from its slice", error);
    return true;
}

bool read_slice(const tiled::Doc& d, const json::Node& s, std::size_t frames, AseSlice& out, std::string& error) {
    if (s.kind != json::Kind::Object) return d.fail(s, "a slice must be an object", error);
    std::string_view name;
    const json::Node* keys = nullptr;
    if (!d.get(s, "name", name, error, tiled::Need::Required) ||
        !d.expect(s, "keys", json::Kind::Array, tiled::Need::Required, keys, error))
        return false;
    out.name = std::string(name);
    out.is_pivot = name == "pivot";
    if (!out.is_pivot && !box_name(name, out.kind, out.index))
        return d.fail(s, "slice '" + out.name + "' is not a box; name boxes " + BOX_NAMES + ", or pivot", error);
    for (const json::Node& k : d.items(*keys)) {
        AseKey key;
        if (!read_key(d, k, out.name, frames, key, error)) return false;
        if (!out.keys.empty() && out.keys.back().frame >= key.frame)
            return d.fail(k, "slice '" + out.name + "' keys must go in frame order, one key a frame", error);
        out.keys.push_back(key);
    }
    return true;
}

} // namespace

bool read_slices(const tiled::Doc& d, const json::Node& meta, std::size_t frames, std::vector<AseSlice>& out,
                 std::string& error) {
    const json::Node* list = nullptr;
    if (!d.expect(meta, "slices", json::Kind::Array, tiled::Need::Optional, list, error)) return false;
    if (list == nullptr) return true;
    for (const json::Node& s : d.items(*list)) {
        AseSlice slice;
        if (!read_slice(d, s, frames, slice, error)) return false;
        for (const AseSlice& seen : out)
            if (seen.name == slice.name) return d.fail(s, "slice '" + slice.name + "' is declared twice", error);
        out.push_back(std::move(slice));
    }
    return true;
}

} // namespace framework::graphics::clip_import
