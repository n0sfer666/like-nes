#include "layer_golden.hpp"
#include "sprite_flip.hpp"
#include "visual_texels.hpp"

namespace layer_golden {
namespace {

int32_t whole(fix32 v) { return v.raw >> 16; }

bool source(const Scene& s, const Sprite& sp, framework::tilemap::TexelRect& r) {
    if (sp.material >= TEXTURES) return false;
    if (sp.region == 0) {
        r = {0, 0, s.textures[sp.material].w, s.textures[sp.material].h};
        return true;
    }
    return framework::tilemap::tile_texels(s.map, static_cast<uint16_t>(sp.region), r);
}

void blit(const Scene& s, const Sprite& sp, bool flips, std::vector<uint8_t>& px) {
    framework::tilemap::TexelRect r;
    if ((sp.rgba & 0xffu) == 0 || !source(s, sp, r)) return;
    const Image& tex = s.textures[sp.material];
    const int32_t x0 = whole(sp.center.x - sp.half.x);
    const int32_t y0 = whole(sp.center.y - sp.half.y);
    const int32_t w = whole(sp.half.x) * 2;
    const int32_t h = whole(sp.half.y) * 2;
    for (int32_t y = y0 < 0 ? 0 : y0; y < y0 + h && y < static_cast<int32_t>(H); ++y) {
        for (int32_t x = x0 < 0 ? 0 : x0; x < x0 + w && x < static_cast<int32_t>(W); ++x) {
            Texel t{static_cast<uint32_t>((x - x0) * static_cast<int32_t>(r.w) / w),
                    static_cast<uint32_t>((y - y0) * static_cast<int32_t>(r.h) / h)};
            if (flips) t = flip_source(sp.flip, r.w, t);
            const uint8_t* src = tex.rgba.data() + 4 * (std::size_t{r.y + t.y} * tex.w + r.x + t.x);
            if (src[3] == 0) continue;
            uint8_t* dst = px.data() + 4 * (static_cast<std::size_t>(y) * W + x);
            for (int c = 0; c < 4; ++c) dst[c] = src[c];
        }
    }
}

} // namespace

std::vector<uint8_t> reference(const Scene& s, const SpriteList& list, bool flips) {
    std::vector<uint8_t> px(std::size_t{4} * W * H);
    for (std::size_t i = 0; i < px.size(); ++i) px[i] = CLEAR[i % 4];
    for (uint32_t i = 0; i < list.count(); ++i) blit(s, list.drawn(i), flips, px);
    return px;
}

} // namespace layer_golden
