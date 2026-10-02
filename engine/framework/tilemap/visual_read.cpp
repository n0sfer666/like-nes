#include "visual_read.hpp"

#include <algorithm>
#include <cstring>

#include "map_bake.hpp"

namespace framework::tilemap {
namespace {

bool tilesets_ok(std::span<const VisualTileset> tilesets, uint32_t& total) {
    uint32_t used = 0;
    for (const VisualTileset& t : tilesets) {
        if (t.first_index != used + 1 || t.tile_count == 0 || t.columns == 0) return false;
        if (t.tile_count > MAX_VISUAL_TILES - used) return false;
        used += t.tile_count;
    }
    total = used;
    return true;
}

bool anims_ok(std::span<const VisualAnim> anims, std::span<const VisualFrame> frames, uint32_t total) {
    uint32_t prev = 0;
    for (const VisualAnim& a : anims) {
        if (a.index <= prev || a.index > total || a.frame_count == 0) return false;
        if (uint64_t{a.first_frame} + a.frame_count > frames.size()) return false;
        uint64_t cycle = 0;
        for (uint32_t k = 0; k < a.frame_count; ++k) {
            const VisualFrame& f = frames[a.first_frame + k];
            if (f.ticks == 0 || f.index == 0 || f.index > total) return false;
            cycle += f.ticks;
        }
        if (cycle != a.cycle_ticks) return false;
        prev = a.index;
    }
    return true;
}

bool layers_ok(const SectionView& v, const VisualRow& r, std::span<const VisualLayer> layers,
               uint32_t total) {
    for (const VisualLayer& l : layers) {
        if (!v.text(l.name_offset) || l.repeat > (REPEAT_X | REPEAT_Y)) return false;
        if (l.kind == static_cast<uint8_t>(LayerKind::Image)) {
            if (l.cells_offset != 0 || l.image_w == 0 || l.image_h == 0) return false;
            continue;
        }
        if (l.kind != static_cast<uint8_t>(LayerKind::Tile)) return false;
        std::span<const uint16_t> cells;
        if (!v.take(l.cells_offset, r.width * r.height, cells)) return false;
        for (uint16_t c : cells)
            if ((c & CELL_INDEX) > total || ((c & CELL_INDEX) == 0 && c != 0)) return false;
    }
    return true;
}

bool row_ok(const SectionView& v, const VisualRow& r) {
    if (r.width == 0 || r.height == 0 || uint64_t{r.width} * r.height > MAX_MAP_TILES) return false;
    if (r.tile_size == 0 || !v.text(r.name_offset)) return false;
    std::span<const VisualTileset> tilesets;
    std::span<const VisualAnim> anims;
    std::span<const VisualFrame> frames;
    std::span<const VisualLayer> layers;
    if (!v.take(r.tileset_offset, r.tileset_count, tilesets) || !v.take(r.anim_offset, r.anim_count, anims) ||
        !v.take(r.frame_offset, r.frame_count, frames) || !v.take(r.layer_offset, r.layer_count, layers))
        return false;
    uint32_t total = 0;
    return tilesets_ok(tilesets, total) && anims_ok(anims, frames, total) &&
           layers_ok(v, r, layers, total);
}

} // namespace

bool VisualTable::open(const void* data, std::size_t size) {
    view_ = SectionView{};
    rows_ = nullptr;
    SectionView v;
    if (!open_section(data, size, VISUAL_MAGIC, VISUAL_VERSION, sizeof(VisualRow),
                      alignof(VisualTileset), v))
        return false;
    const std::span<const VisualRow> rows = v.at<VisualRow>(v.header->rows_offset, v.header->count);
    for (const VisualRow& r : rows)
        if (!row_ok(v, r)) return false;
    view_ = v;
    rows_ = rows.data();
    return true;
}

const char* VisualTable::name(uint32_t index) const {
    if (index >= count()) return "";
    return view_.strings + rows_[index].name_offset;
}

VisualMap VisualTable::map(uint32_t index) const {
    if (index >= count()) return {};
    const VisualRow& r = rows_[index];
    VisualMap m;
    m.row = &r;
    m.tilesets = view_.at<VisualTileset>(r.tileset_offset, r.tileset_count);
    m.anims = view_.at<VisualAnim>(r.anim_offset, r.anim_count);
    m.frames = view_.at<VisualFrame>(r.frame_offset, r.frame_count);
    m.layers = view_.at<VisualLayer>(r.layer_offset, r.layer_count);
    m.base = view_.base;
    m.strings = view_.strings;
    return m;
}

VisualMap VisualTable::find(const char* wanted) const {
    if (wanted == nullptr) return {};
    for (uint32_t i = 0; i < count(); ++i)
        if (std::strcmp(name(i), wanted) == 0) return map(i);
    return {};
}

const char* layer_name(const VisualMap& map, const VisualLayer& layer) {
    return map.strings == nullptr ? "" : map.strings + layer.name_offset;
}

std::span<const uint16_t> layer_cells(const VisualMap& map, const VisualLayer& layer) {
    if (map.row == nullptr || layer.kind != static_cast<uint8_t>(LayerKind::Tile)) return {};
    // Zero-parse: блок клеток проверен `VisualTable::open` на границы и выравнивание (ADR 0003).
    const auto* cells = reinterpret_cast<const uint16_t*>(map.base + layer.cells_offset);
    return {cells, std::size_t{map.row->width} * map.row->height};
}

const VisualTileset* tileset_of(const VisualMap& map, uint16_t index) {
    for (const VisualTileset& t : map.tilesets)
        if (index >= t.first_index && index - t.first_index < t.tile_count) return &t;
    return nullptr;
}

uint16_t visual_region(const VisualMap& map, uint16_t cell, uint32_t tick) {
    const uint16_t index = cell & CELL_INDEX;
    const auto it = std::lower_bound(map.anims.begin(), map.anims.end(), index,
                                     [](const VisualAnim& a, uint16_t i) { return a.index < i; });
    if (index == 0 || it == map.anims.end() || it->index != index) return cell;
    uint32_t t = tick % it->cycle_ticks;
    for (uint32_t k = 0; k < it->frame_count; ++k) {
        const VisualFrame& f = map.frames[it->first_frame + k];
        if (t < f.ticks) return static_cast<uint16_t>((cell & ~CELL_INDEX) | f.index);
        t -= f.ticks;
    }
    return cell;
}

} // namespace framework::tilemap
