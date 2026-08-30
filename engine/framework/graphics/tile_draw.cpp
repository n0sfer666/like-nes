#include "tile_draw.hpp"

namespace framework::graphics {
namespace {

const fix32 HALF = fix32::from_raw(fix32::ONE / 2);

} // namespace

TileDrawStats draw_tiles(SpriteList& out, const tilemap::TileGrid& grid, const physics::Aabb& view,
                         const TileSet& set) {
    TileDrawStats stats;
    const tilemap::TileWindow win = grid.window(view);
    // Пустое окно отдельной проверки не получает намеренно: оба цикла строгие, а `TileWindow::empty`
    // это ровно `x1 <= x0 || y1 <= y0`, — ранний выход был бы строкой, удаление которой не замечает
    // ни один гейт, то есть украшением.
    //
    // Обход построчный, сверху вниз: порядок подачи и есть порядок отрисовки внутри одного слоя и
    // материала (шаг A), поэтому он наблюдаем — и голден его пинит.
    for (int32_t y = win.y0; y < win.y1; ++y) {
        for (int32_t x = win.x0; x < win.x1; ++x) {
            ++stats.visited;
            const tilemap::TileFlags flags = grid.at(x, y);
            if (flags >= TILE_KINDS) {
                ++stats.unknown;
                continue;
            }
            const RegionId region = set.region[flags];
            if (region == 0) continue;
            const physics::Aabb box = grid.tile_bounds(x, y);
            Sprite s;
            s.center = (box.min + box.max) * HALF;
            s.half = (box.max - box.min) * HALF;
            s.rgba = set.rgba;
            s.region = region;
            s.material = set.material;
            s.layer = set.layer;
            out.push(s);
            ++stats.emitted;
        }
    }
    return stats;
}

} // namespace framework::graphics
