#include "visual_bake.hpp"

#include <algorithm>

#include "section_bake.hpp"
#include "visual_read.hpp"

namespace framework::tilemap {
namespace {

bool refuse(std::string& error, const std::string& message) {
    error = message;
    return false;
}

bool check(std::span<const VisualMapSrc> maps, std::string& error) {
    if (maps.empty()) return refuse(error, "no visual map to bake");
    for (std::size_t i = 0; i < maps.size(); ++i) {
        const VisualMapSrc& m = maps[i];
        if (m.name.empty()) return refuse(error, "a visual map needs a name");
        for (std::size_t j = 0; j < i; ++j)
            if (maps[j].name == m.name) return refuse(error, "visual map '" + m.name + "' is declared twice");
        for (const VisualLayerSrc& l : m.layers) {
            const std::size_t want = l.kind == LayerKind::Tile ? std::size_t{m.width} * m.height : 0;
            if (l.cells.size() != want)
                return refuse(error, "layer '" + l.name + "' of '" + m.name + "' has cells for a different size");
        }
    }
    return true;
}

bool anims(core::SectionBuilder& b, const VisualMapSrc& m, VisualRow& row, std::string& error) {
    std::vector<const VisualAnimSrc*> order;
    for (const VisualAnimSrc& a : m.anims) order.push_back(&a);
    std::sort(order.begin(), order.end(),
              [](const VisualAnimSrc* x, const VisualAnimSrc* y) { return x->index < y->index; });
    std::vector<VisualAnim> table;
    std::vector<VisualFrame> frames;
    for (const VisualAnimSrc* a : order) {
        uint64_t cycle = 0;
        for (const VisualFrame& f : a->frames) cycle += f.ticks;
        if (a->frames.size() > 0xFFFFu || cycle > 0xFFFFFFFFull)
            return refuse(error, "animation of tile " + std::to_string(a->index) + " in '" + m.name +
                                     "' is too long for the format");
        table.push_back(VisualAnim{a->index, static_cast<uint16_t>(a->frames.size()),
                                   static_cast<uint32_t>(frames.size()), static_cast<uint32_t>(cycle)});
        frames.insert(frames.end(), a->frames.begin(), a->frames.end());
    }
    row.anim_offset = b.block(table.data(), table.size() * sizeof(VisualAnim), alignof(VisualAnim));
    row.anim_count = static_cast<uint32_t>(table.size());
    row.frame_offset = b.block(frames.data(), frames.size() * sizeof(VisualFrame), alignof(VisualFrame));
    row.frame_count = static_cast<uint32_t>(frames.size());
    return true;
}

void layers(core::SectionBuilder& b, const VisualMapSrc& m, VisualRow& row) {
    std::vector<VisualLayer> table;
    for (const VisualLayerSrc& l : m.layers) {
        VisualLayer out{};
        out.name_offset = b.text(l.name);
        out.kind = static_cast<uint8_t>(l.kind);
        out.opacity = l.opacity;
        out.repeat = l.repeat;
        out.parallax_x_raw = l.parallax_x.raw;
        out.parallax_y_raw = l.parallax_y.raw;
        out.offset_x_raw = l.offset_x.raw;
        out.offset_y_raw = l.offset_y.raw;
        if (l.kind == LayerKind::Tile)
            out.cells_offset = b.block(l.cells.data(), l.cells.size() * sizeof(uint16_t), 4);
        out.image_w = l.image_w;
        out.image_h = l.image_h;
        out.image_guid = l.image_guid;
        table.push_back(out);
    }
    row.layer_offset = b.block(table.data(), table.size() * sizeof(VisualLayer), alignof(VisualLayer));
    row.layer_count = static_cast<uint32_t>(table.size());
}

} // namespace

bool bake_visuals(std::span<const VisualMapSrc> maps, std::vector<uint8_t>& out, std::string& error) {
    if (!check(maps, error)) return false;
    std::vector<VisualRow> rows(maps.size(), VisualRow{});
    core::SectionBuilder b(rows.size() * sizeof(VisualRow));
    for (std::size_t i = 0; i < maps.size(); ++i) {
        const VisualMapSrc& m = maps[i];
        VisualRow& r = rows[i];
        r.name_offset = b.text(m.name);
        r.width = m.width;
        r.height = m.height;
        r.tile_size = m.tile_size;
        r.background_rgba = m.background_rgba;
        r.tileset_offset = b.block(m.tilesets.data(), m.tilesets.size() * sizeof(VisualTileset),
                                   alignof(VisualTileset));
        r.tileset_count = static_cast<uint32_t>(m.tilesets.size());
        if (!anims(b, m, r, error)) return false;
        layers(b, m, r);
    }
    if (!b.finish(VISUAL_MAGIC, VISUAL_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), out, error))
        return false;
    VisualTable probe;
    if (!probe.open(out.data(), out.size()))
        return refuse(error, "baked visual table fails its own reader (tileset ranges, animation "
                             "frames or cell indices out of order)");
    return true;
}

} // namespace framework::tilemap
