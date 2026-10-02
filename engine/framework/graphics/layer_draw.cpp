#include "layer_draw.hpp"

#include <algorithm>

#include "visual_format.hpp"

namespace framework::graphics {
namespace {

using tilemap::VisualLayer;
using tilemap::VisualMap;

static_assert(SPRITE_FLIP_D == (tilemap::CELL_FLIP_D >> 13) &&
                  SPRITE_FLIP_V == (tilemap::CELL_FLIP_V >> 13) &&
                  SPRITE_FLIP_H == (tilemap::CELL_FLIP_H >> 13),
              "sprite flip bits mirror LNVL cell bits");

int64_t floor_div(int64_t a, int64_t b) {
    const int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

struct Range {
    int64_t first = 0;
    int64_t last = 0;
};

// Повтор не копирует слой целиком: окно считается в «глобальных» клетках, бесконечных в обе
// стороны, и клетка берётся по модулю. Цена кадра поэтому — мера экрана, а не число копий.
Range visible(fix32 origin, fix32 step, fix32 screen, uint32_t cells, bool repeat) {
    Range r{floor_div(-static_cast<int64_t>(origin.raw), step.raw),
            -floor_div(static_cast<int64_t>(origin.raw) - screen.raw, step.raw)};
    if (!repeat) {
        r.first = std::max<int64_t>(r.first, 0);
        r.last = std::min<int64_t>(r.last, cells);
    }
    return r;
}

uint32_t wrap(int64_t g, uint32_t n) {
    const int64_t m = g % n;
    return static_cast<uint32_t>(m < 0 ? m + n : m);
}

fix32 cell_center(fix32 origin, int64_t g, fix32 step) {
    return fix32::from_raw(fix32::sat(origin.raw + g * step.raw + step.raw / 2));
}

bool material_of(std::span<const uint64_t> textures, uint64_t guid, uint16_t& out) {
    const auto it = std::find(textures.begin(), textures.end(), guid);
    if (it == textures.end()) return false;
    out = static_cast<uint16_t>(it - textures.begin());
    return true;
}

struct Grid {
    Vec2 origin;
    Vec2 step;
    uint32_t cols = 0;
    uint32_t rows = 0;
};

bool cell_sprite(const VisualMap& map, const VisualLayer& layer, const LayerFrame& f,
                 uint32_t x, uint32_t y, Sprite& s, LayerDrawStats& stats) {
    if (layer.kind == static_cast<uint8_t>(tilemap::LayerKind::Image)) {
        if (material_of(f.textures, layer.image_guid, s.material)) return true;
        ++stats.unknown;
        return false;
    }
    const uint16_t raw = tilemap::layer_cells(map, layer)[std::size_t{y} * map.row->width + x];
    if ((raw & tilemap::CELL_INDEX) == 0) return false;
    const uint16_t shown = tilemap::visual_region(map, raw, static_cast<uint32_t>(f.tick));
    const uint16_t index = shown & tilemap::CELL_INDEX;
    const tilemap::VisualTileset* t = tilemap::tileset_of(map, index);
    if (t == nullptr || !material_of(f.textures, t->texture_guid, s.material)) {
        ++stats.unknown;
        return false;
    }
    s.region = index;
    s.flip = static_cast<uint8_t>(shown >> 13);
    return true;
}

} // namespace

LayerDrawStats draw_layer(SpriteList& out, const VisualMap& map, const VisualLayer& layer,
                          const LayerFrame& f, int16_t order) {
    LayerDrawStats stats;
    const fix32 scale = viewport_scale(f.view);
    if (map.row == nullptr || scale.raw == 0) return stats;
    const bool image = layer.kind == static_cast<uint8_t>(tilemap::LayerKind::Image);
    const Vec2 parallax{fix32::from_raw(layer.parallax_x_raw),
                        fix32::from_raw(layer.parallax_y_raw)};
    const Vec2 center = camera_layer_center(f.camera, f.config, f.tick, parallax);
    const Vec2 offset{fix32::from_raw(layer.offset_x_raw), fix32::from_raw(layer.offset_y_raw)};
    Grid g;
    g.origin = world_to_screen_snapped(f.view, center, offset);
    const uint32_t w = image ? layer.image_w : map.row->tile_size;
    const uint32_t h = image ? layer.image_h : map.row->tile_size;
    g.step = {fix32::from_int(static_cast<int32_t>(w)) * scale,
              fix32::from_int(static_cast<int32_t>(h)) * scale};
    g.cols = image ? 1 : map.row->width;
    g.rows = image ? 1 : map.row->height;
    if (g.step.x.raw <= 0 || g.step.y.raw <= 0 || g.cols == 0 || g.rows == 0) return stats;
    const Vec2 screen = f.view.screen_half * fix32::from_int(2);
    const bool repeat_x = (layer.repeat & tilemap::REPEAT_X) != 0;
    const bool repeat_y = (layer.repeat & tilemap::REPEAT_Y) != 0;
    if ((repeat_x && g.step.x.raw < fix32::ONE) || (repeat_y && g.step.y.raw < fix32::ONE)) {
        stats.truncated = true;
        return stats;
    }
    const Range rx = visible(g.origin.x, g.step.x, screen.x, g.cols, repeat_x);
    const Range ry = visible(g.origin.y, g.step.y, screen.y, g.rows, repeat_y);
    for (int64_t gy = ry.first; gy < ry.last; ++gy) {
        for (int64_t gx = rx.first; gx < rx.last; ++gx) {
            ++stats.visited;
            Sprite s;
            if (!cell_sprite(map, layer, f, wrap(gx, g.cols), wrap(gy, g.rows), s, stats)) continue;
            s.center = {cell_center(g.origin.x, gx, g.step.x), cell_center(g.origin.y, gy, g.step.y)};
            s.half = {fix32::from_raw(g.step.x.raw / 2), fix32::from_raw(g.step.y.raw / 2)};
            s.rgba = 0xffffff00u | layer.opacity;
            s.layer = order;
            const bool full = out.full();
            out.push(s);
            if (full) {
                stats.truncated = true;
                return stats;
            }
            ++stats.emitted;
        }
    }
    return stats;
}

} // namespace framework::graphics
