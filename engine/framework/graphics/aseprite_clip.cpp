#include <algorithm>

#include "aseprite_parts.hpp"

namespace framework::graphics::clip_import {
namespace {

const AseKey* active(const AseSlice& s, uint32_t from, uint32_t frame) {
    const AseKey* hit = nullptr;
    for (const AseKey& k : s.keys)
        if (k.frame >= from && k.frame <= frame) hit = &k;
    return hit == nullptr || box_off(hit->bounds) ? nullptr : hit;
}

bool inside(const Box& b, const AseFrame& f) { return b.x + b.w <= f.src_w && b.y + b.h <= f.src_h; }

bool frame_of(const tiled::Doc& d, const AseTag& t, uint32_t i, const AseFrame& a, std::span<const AseSlice> slices,
              ClipFrameSrc& c, std::string& error) {
    Pixel pivot{a.src_w / 2, a.src_h};
    for (const AseSlice& s : slices) {
        const AseKey* k = s.is_pivot ? active(s, t.from, i) : nullptr;
        if (k == nullptr) continue;
        const Box& b = k->bounds;
        pivot = k->has_pivot ? Pixel{b.x + k->pivot.x, b.y + k->pivot.y} : Pixel{b.x + b.w / 2, b.y + b.h / 2};
        if (pivot.x < 0 || pivot.y < 0 || pivot.x > a.src_w || pivot.y > a.src_h)
            return d.fail(*k->at, "the pivot on frame " + std::to_string(i) + " lies outside the frame", error);
    }
    c = ClipFrameSrc{a.x, a.y, a.w, a.h, 0, 0, a.ticks, t.events[i - t.from], {}};
    place(pivot, a.trim, c);
    for (const AseSlice& s : slices) {
        const AseKey* k = s.is_pivot ? nullptr : active(s, t.from, i);
        if (k == nullptr) continue;
        if (!inside(k->bounds, a))
            return d.fail(*k->at, "slice '" + s.name + "' on frame " + std::to_string(i) + " lies outside the " +
                                      std::to_string(a.src_w) + "x" + std::to_string(a.src_h) + " frame",
                          error);
        add_box(c, s.kind, s.index, pivot, k->bounds);
    }
    return true;
}

} // namespace

bool build_clip(const tiled::Doc& d, const AseTag& tag, std::span<const AseFrame> frames,
                std::span<const AseSlice> slices, std::vector<ClipFrameSrc>& out, std::string& error) {
    out.assign(tag.to - tag.from + 1, ClipFrameSrc{});
    for (uint32_t i = tag.from; i <= tag.to; ++i)
        if (!frame_of(d, tag, i, frames[i], slices, out[i - tag.from], error)) return false;
    if (tag.reverse) std::reverse(out.begin(), out.end());
    return true;
}

} // namespace framework::graphics::clip_import
