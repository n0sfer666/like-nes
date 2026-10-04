#include "layer_cover.hpp"

#include <algorithm>
#include <cstdint>

#include "camera.hpp"
#include "viewport_fit.hpp"

namespace framework::graphics {
namespace {

using tilemap::VisualLayerSrc;

constexpr int64_t ONE = fix32::ONE;

struct Span {
    int64_t lo = 0;
    int64_t hi = 0;
};

int64_t floor_div(int64_t a, int64_t b) {
    const int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

int64_t ceil_px(int64_t raw) { return -floor_div(-raw, ONE); }

// Тот же отрезок центров, что режет `camera_follow` при `half_view` = `view_zone_half`, включая
// уровень уже зоны (центр посередине), и шире на потолок тряски: она сдвигает вид ПОСЛЕ клампа.
Span centers(Span bounds, fix32 half) {
    Span c{bounds.lo + half.raw, bounds.hi - half.raw};
    if (c.hi < c.lo) c.lo = c.hi = (bounds.lo + bounds.hi) / 2;
    return {c.lo - CAMERA_SHAKE_MAX.raw, c.hi + CAMERA_SHAKE_MAX.raw};
}

bool level_bounds(const tilemap::VisualMapSrc& map, const tilemap::ObjectMapSrc& objects, Span& x,
                  Span& y, std::string& error) {
    x = {0, int64_t{map.width} * map.tile_size * ONE};
    y = {0, int64_t{map.height} * map.tile_size * ONE};
    const tilemap::ObjectSrc* found = nullptr;
    for (const tilemap::ObjectSrc& o : objects.objects) {
        if (o.cls != "bounds") continue;
        if (found != nullptr) {
            error = "map '" + map.name + "' has two objects of class bounds";
            return false;
        }
        if (o.shape != tilemap::ObjectShape::Rect) {
            error = "map '" + map.name + "': bounds object '" + o.name + "' must be a rectangle";
            return false;
        }
        found = &o;
    }
    if (found == nullptr) return true;
    x = {found->x.raw, int64_t{found->x.raw} + found->w.raw};
    y = {found->y.raw, int64_t{found->y.raw} + found->h.raw};
    return true;
}

struct Short {
    int64_t lo = 0;
    int64_t hi = 0;
};

// Полоса в координатах СЛОЯ: слой стоит на `offset` от центра отрисовки `c·p`, поэтому смещение
// сдвигает полосу в обратную сторону. Нижний край — вниз до целого мирового пикселя:
// `CAMERA_PIXEL_PERFECT` округляет центр слоя вниз на сетку не крупнее пикселя, и при дробном
// смещении дыра в пиксель прошла бы. Верхний край привязка только отодвигает, ему хватает raw.
Short shortfall(Span c, int32_t parallax, int32_t offset, int64_t size_px, uint32_t limit) {
    const int64_t a = c.lo * parallax;
    const int64_t b = c.hi * parallax;
    const int64_t half = int64_t{limit} * ONE / 2;
    const int64_t lo = floor_div(std::min(a, b), ONE * ONE) * ONE - half - offset;
    const int64_t hi = -floor_div(-std::max(a, b), ONE) + half - offset;
    return {lo < 0 ? ceil_px(-lo) : 0, hi > size_px * ONE ? ceil_px(hi - size_px * ONE) : 0};
}

void describe(std::string& out, const char* axis, Short s, const char* lo_side, const char* hi_side) {
    if (s.lo == 0 && s.hi == 0) return;
    out += out.empty() ? "" : "; ";
    out += std::string(axis) + " short by";
    if (s.lo > 0) out += " " + std::to_string(s.lo) + " px " + lo_side;
    if (s.lo > 0 && s.hi > 0) out += ",";
    if (s.hi > 0) out += " " + std::to_string(s.hi) + " px " + hi_side;
}

bool layer_covers(const tilemap::VisualMapSrc& map, const VisualLayerSrc& l, Span cx, Span cy,
                  std::string& error) {
    const std::string who = "map '" + map.name + "': layer '" + l.name + "'";
    const bool rx = (l.repeat & tilemap::REPEAT_X) != 0;
    const bool ry = (l.repeat & tilemap::REPEAT_Y) != 0;
    if (!rx && l.parallax_x.raw < ONE) {
        error = who + " has parallax x below 1 and must repeat on x";
        return false;
    }
    const bool image = l.kind == tilemap::LayerKind::Image;
    const int64_t w = image ? l.image_w : int64_t{map.width} * map.tile_size;
    const int64_t h = image ? l.image_h : int64_t{map.height} * map.tile_size;
    std::string gaps;
    if (!rx)
        describe(gaps, "x", shortfall(cx, l.parallax_x.raw, l.offset_x.raw, w, VIEW_LIMIT.w), "left",
                 "right");
    if (!ry)
        describe(gaps, "y", shortfall(cy, l.parallax_y.raw, l.offset_y.raw, h, VIEW_LIMIT.h), "top",
                 "bottom");
    if (gaps.empty()) return true;
    error = who + " does not cover the view: " + gaps;
    return false;
}

} // namespace

bool check_layer_cover(const tilemap::VisualMapSrc& map, const tilemap::ObjectMapSrc& objects,
                       std::string& error) {
    Span bx, by;
    if (!level_bounds(map, objects, bx, by, error)) return false;
    const Vec2 half = view_zone_half();
    const Span cx = centers(bx, half.x);
    const Span cy = centers(by, half.y);
    for (const VisualLayerSrc& l : map.layers)
        if (!layer_covers(map, l, cx, cy, error)) return false;
    return true;
}

} // namespace framework::graphics
