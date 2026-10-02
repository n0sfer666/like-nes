#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "section_open.hpp"
#include "visual_format.hpp"

namespace framework::tilemap {

struct VisualMap {
    const VisualRow* row = nullptr;
    std::span<const VisualTileset> tilesets;
    std::span<const VisualAnim> anims;
    std::span<const VisualFrame> frames;
    std::span<const VisualLayer> layers;
    const uint8_t* base = nullptr;
    const char* strings = nullptr;
};

class VisualTable {
public:
    bool open(const void* data, std::size_t size);
    bool valid() const { return view_.header != nullptr; }
    uint32_t count() const { return valid() ? view_.header->count : 0; }
    const char* name(uint32_t index) const;
    VisualMap map(uint32_t index) const;
    VisualMap find(const char* name) const;

private:
    SectionView view_;
    const VisualRow* rows_ = nullptr;
};

const char* layer_name(const VisualMap& map, const VisualLayer& layer);
std::span<const uint16_t> layer_cells(const VisualMap& map, const VisualLayer& layer);
const VisualTileset* tileset_of(const VisualMap& map, uint16_t index);
uint16_t visual_region(const VisualMap& map, uint16_t cell, uint32_t tick);

} // namespace framework::tilemap
