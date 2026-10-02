#pragma once
#include <cstdint>

#include "section_format.hpp"

namespace framework::tilemap {

constexpr uint8_t VISUAL_MAGIC[4] = {'L', 'N', 'V', 'L'};
constexpr uint32_t VISUAL_VERSION = 1;

constexpr uint16_t CELL_INDEX = 0x1FFF;
constexpr uint16_t CELL_FLIP_D = 1u << 13;
constexpr uint16_t CELL_FLIP_V = 1u << 14;
constexpr uint16_t CELL_FLIP_H = 1u << 15;
constexpr uint32_t MAX_VISUAL_TILES = CELL_INDEX;

enum class LayerKind : uint8_t { Tile = 0, Image = 1 };
constexpr uint8_t REPEAT_X = 1u << 0;
constexpr uint8_t REPEAT_Y = 1u << 1;

struct VisualRow {
    uint32_t name_offset;
    uint32_t width;
    uint32_t height;
    uint32_t tile_size;
    uint32_t background_rgba;
    uint32_t tileset_offset;
    uint32_t tileset_count;
    uint32_t anim_offset;
    uint32_t anim_count;
    uint32_t frame_offset;
    uint32_t frame_count;
    uint32_t layer_offset;
    uint32_t layer_count;
    uint32_t pad;
};
static_assert(sizeof(VisualRow) == 56, "VisualRow layout pinned (zero-parse ABI)");

struct VisualTileset {
    uint64_t texture_guid;
    uint32_t first_index;
    uint32_t tile_count;
    uint32_t columns;
    uint32_t margin;
    uint32_t spacing;
    uint32_t pad;
};
static_assert(sizeof(VisualTileset) == 32, "VisualTileset layout pinned (zero-parse ABI)");

struct VisualAnim {
    uint16_t index;
    uint16_t frame_count;
    uint32_t first_frame;
    uint32_t cycle_ticks;
};
static_assert(sizeof(VisualAnim) == 12, "VisualAnim layout pinned (zero-parse ABI)");

struct VisualFrame {
    uint16_t index;
    uint16_t ticks;
};
static_assert(sizeof(VisualFrame) == 4, "VisualFrame layout pinned (zero-parse ABI)");

struct VisualLayer {
    uint32_t name_offset;
    uint8_t kind;
    uint8_t opacity;
    uint8_t repeat;
    uint8_t pad0;
    int32_t parallax_x_raw;
    int32_t parallax_y_raw;
    int32_t offset_x_raw;
    int32_t offset_y_raw;
    uint32_t cells_offset;
    uint32_t image_w;
    uint32_t image_h;
    uint32_t pad1;
    uint64_t image_guid;
};
static_assert(sizeof(VisualLayer) == 48, "VisualLayer layout pinned (zero-parse ABI)");

} // namespace framework::tilemap
