#include "visual_texels.hpp"

namespace framework::tilemap {

bool tile_texels(const VisualMap& map, uint16_t index, TexelRect& out) {
    const VisualTileset* t = map.row == nullptr ? nullptr : tileset_of(map, index & CELL_INDEX);
    if (t == nullptr || t->columns == 0) return false;
    const uint32_t local = (index & CELL_INDEX) - t->first_index;
    const uint32_t ts = map.row->tile_size;
    out.x = t->margin + (local % t->columns) * (ts + t->spacing);
    out.y = t->margin + (local / t->columns) * (ts + t->spacing);
    out.w = ts;
    out.h = ts;
    return true;
}

} // namespace framework::tilemap
