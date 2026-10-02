#pragma once
#include <cstdint>

#include "clip.hpp"

namespace framework::graphics {

constexpr uint8_t CLIP_MAGIC[4] = {'L', 'N', 'C', 'L'};
constexpr uint32_t CLIP_VERSION = 1;
constexpr uint8_t MAX_BOXES_OF_KIND = 4;
constexpr uint16_t CLIP_FLAGS_ALL = CLIP_LOOP | CLIP_PINGPONG;

enum class BoxKind : uint8_t { Hit = 0, Hurt = 1, Push = 2 };

struct Rect16 {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};
static_assert(sizeof(Rect16) == 8, "Rect16 layout pinned (zero-parse ABI)");

struct ClipRow {
    uint32_t name_offset;
    uint16_t flags;
    uint16_t frame_count;
    uint64_t texture_guid;
    uint32_t frame_offset;
    uint32_t cel_offset;
    uint32_t box_offset;
    uint32_t box_count;
    uint32_t event_offset;
    uint32_t event_count;
};
static_assert(sizeof(ClipRow) == 40, "ClipRow layout pinned (zero-parse ABI)");

struct ClipCel {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    int16_t anchor_x;
    int16_t anchor_y;
    uint16_t first_box;
    uint16_t box_count;
};
static_assert(sizeof(ClipCel) == 16, "ClipCel layout pinned (zero-parse ABI)");

struct ClipBox {
    uint8_t kind;
    uint8_t index;
    uint16_t pad;
    Rect16 rect;
};
static_assert(sizeof(ClipBox) == 12, "ClipBox layout pinned (zero-parse ABI)");

static_assert(sizeof(ClipFrame) == 6, "ClipFrame layout pinned (zero-parse ABI)");

inline uint8_t box_limit(uint8_t kind) {
    return kind == static_cast<uint8_t>(BoxKind::Push) ? 1 : MAX_BOXES_OF_KIND;
}

} // namespace framework::graphics
