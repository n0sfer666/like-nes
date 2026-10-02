#include <algorithm>

#include "clip_golden.hpp"

namespace clip_golden {
namespace {

constexpr uint32_t CEL_RGBA = 0x00ffffffu;
constexpr uint32_t PIVOT_RGBA = 0xffffffffu;
constexpr uint32_t BOX_RGBA[3] = {0xff0000ffu, 0x00ff00ffu, 0xffff00ffu};

struct Span {
    int32_t from;
    int32_t to;
};

struct Canvas {
    std::vector<uint8_t> px;

    void put(int32_t x, int32_t y, const uint8_t c[4]) {
        if (x < 0 || y < 0 || x >= static_cast<int32_t>(W) || y >= static_cast<int32_t>(H)) return;
        std::copy(c, c + 4, px.begin() + 4 * (static_cast<std::ptrdiff_t>(y) * W + x));
    }

    void block(Span xs, Span ys, const uint8_t c[4]) {
        for (int32_t y = ys.from; y < ys.to; ++y)
            for (int32_t x = xs.from; x < xs.to; ++x) put(x, y, c);
    }

    void border(Span xs, Span ys, uint32_t rgba) {
        const uint8_t c[4] = {static_cast<uint8_t>(rgba >> 24), static_cast<uint8_t>(rgba >> 16),
                              static_cast<uint8_t>(rgba >> 8), static_cast<uint8_t>(rgba)};
        for (int32_t y = ys.from; y < ys.to; ++y)
            for (int32_t x = xs.from; x < xs.to; ++x)
                if (x == xs.from || x == xs.to - 1 || y == ys.from || y == ys.to - 1) put(x, y, c);
    }
};

Span along(int32_t pivot, int32_t from, int32_t to, int32_t zoom, bool mirror) {
    return mirror ? Span{pivot - to * zoom, pivot - from * zoom} : Span{pivot + from * zoom, pivot + to * zoom};
}

const ClipFrameSrc& frame_at(const ClipSrc& src, uint64_t tick) {
    uint64_t period = 0;
    for (const ClipFrameSrc& f : src.frames) period += f.duration;
    uint64_t t = tick % period;
    for (const ClipFrameSrc& f : src.frames) {
        if (t < f.duration) return f;
        t -= f.duration;
    }
    return src.frames.back();
}

void draw_cel(Canvas& c, const Image& sheet, const ClipFrameSrc& f, const CelPlace& at, bool flip) {
    for (int32_t r = 0; r < f.h; ++r)
        for (int32_t k = 0; k < f.w; ++k) {
            const uint8_t* t = sheet.rgba.data() + 4 * ((static_cast<std::size_t>(f.y) + r) * sheet.w + f.x + k);
            if (t[3] == 0) continue;
            c.block(along(at.x, k - f.anchor_x, k - f.anchor_x + 1, at.zoom, flip),
                    along(at.y, r - f.anchor_y, r - f.anchor_y + 1, at.zoom, false), t);
        }
}

void draw_overlay(Canvas& c, const ClipFrameSrc& f, const CelPlace& at, bool flip, bool boxes) {
    c.border(along(at.x, -f.anchor_x, f.w - f.anchor_x, at.zoom, flip),
             along(at.y, -f.anchor_y, f.h - f.anchor_y, at.zoom, false), CEL_RGBA);
    std::vector<ClipBoxSrc> sorted = f.boxes;
    std::stable_sort(sorted.begin(), sorted.end(), [](const ClipBoxSrc& a, const ClipBoxSrc& b) {
        return a.kind != b.kind ? a.kind < b.kind : a.index < b.index;
    });
    for (const ClipBoxSrc& b : boxes ? sorted : std::vector<ClipBoxSrc>{})
        c.border(along(at.x, b.rect.x, b.rect.x + b.rect.w, at.zoom, flip),
                 along(at.y, b.rect.y, b.rect.y + b.rect.h, at.zoom, false), BOX_RGBA[static_cast<uint8_t>(b.kind)]);
    c.border({at.x - 2, at.x + 3}, {at.y, at.y + 1}, PIVOT_RGBA);
    c.border({at.x, at.x + 1}, {at.y - 2, at.y + 3}, PIVOT_RGBA);
}

} // namespace

std::vector<uint8_t> reference(const Scene& s, const View& v, Blind blind) {
    Canvas c;
    c.px.resize(std::size_t{4} * W * H);
    for (std::size_t i = 0; i < c.px.size(); ++i) c.px[i] = CLEAR[i % 4];
    const ClipFrameSrc& f = frame_at(s.src, v.tick);
    const bool flip = v.at.flip_h && blind != Blind::Flip;
    draw_cel(c, s.sheet, f, v.at, flip);
    draw_overlay(c, f, v.at, flip, blind != Blind::Boxes);
    return c.px;
}

} // namespace clip_golden
