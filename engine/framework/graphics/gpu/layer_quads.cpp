#include "layer_quads.hpp"

#include "visual_texels.hpp"

namespace framework::graphics {
namespace {

float px(fix32 v) { return static_cast<float>(v.raw) / static_cast<float>(fix32::ONE); }

bool source_of(const Sprite& s, const tilemap::VisualMap& map, std::span<const TextureSize> sizes,
               tilemap::TexelRect& r) {
    if (s.material >= sizes.size()) return false;
    const TextureSize t = sizes[s.material];
    if (s.region == 0) {
        r = {0, 0, t.w, t.h};
    } else if (!tilemap::tile_texels(map, static_cast<uint16_t>(s.region), r)) {
        return false;
    }
    return r.w != 0 && r.h != 0 && r.x <= t.w && r.w <= t.w - r.x && r.y <= t.h &&
           r.h <= t.h - r.y;
}

} // namespace

QuadStats layer_quads(const SpriteList& list, std::span<const Batch> batches,
                      const tilemap::VisualMap& map, std::span<const TextureSize> sizes,
                      std::span<render::Quad> quads, std::span<render::QuadRun> runs) {
    QuadStats st;
    for (const Batch& b : batches) {
        if (st.runs == runs.size() || b.count > quads.size() - st.quads) {
            st.dropped += b.count;
            continue;
        }
        runs[st.runs++] = {st.quads, b.count, b.material};
        for (uint32_t i = b.first; i < b.first + b.count; ++i) {
            const Sprite& s = list.drawn(i);
            render::Quad& q = quads[st.quads++];
            q = render::Quad{};
            tilemap::TexelRect r;
            if (!source_of(s, map, sizes, r)) {
                ++st.rejected;
                continue;
            }
            q.x = px(s.center.x - s.half.x);
            q.y = px(s.center.y - s.half.y);
            q.w = px(s.half.x) * 2;
            q.h = px(s.half.y) * 2;
            q.tx = static_cast<float>(r.x);
            q.ty = static_cast<float>(r.y);
            q.tw = static_cast<float>(r.w);
            q.th = static_cast<float>(r.h);
            q.rgba = s.rgba;
            q.flip = s.flip;
        }
    }
    return st;
}

} // namespace framework::graphics
