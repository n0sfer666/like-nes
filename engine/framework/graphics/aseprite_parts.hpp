#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "clip_parts.hpp"
#include "tiled_import.hpp"
#include "tmj_doc.hpp"

namespace framework::graphics::clip_import {

struct AseFrame {
    uint16_t x = 0, y = 0, w = 0, h = 0;
    Pixel trim;
    int64_t src_w = 0, src_h = 0;
    uint16_t ticks = 1;
};

struct AseTag {
    std::string name;
    uint32_t from = 0, to = 0;
    bool reverse = false;
    uint16_t flags = CLIP_LOOP;
    std::vector<std::string> events;
};

struct AseKey {
    const json::Node* at = nullptr;
    uint32_t frame = 0;
    Box bounds;
    bool has_pivot = false;
    Pixel pivot;
};

struct AseSlice {
    std::string name;
    bool is_pivot = false;
    BoxKind kind = BoxKind::Hit;
    uint8_t index = 0;
    std::vector<AseKey> keys;
};

inline bool box_off(const Box& b) { return b.w == 0 && b.h == 0; }

bool read_box(const tiled::Doc& d, const json::Node& obj, std::string_view key, bool size_only, Box& out,
              std::string& error);
bool read_frames(const tiled::Doc& d, const json::Node& frames, const tiled::Image& sheet,
                 std::vector<AseFrame>& out, std::string& error);
bool read_tags(const tiled::Doc& d, const json::Node& meta, std::size_t frames, std::vector<AseTag>& out,
               std::string& error);
bool read_slices(const tiled::Doc& d, const json::Node& meta, std::size_t frames, std::vector<AseSlice>& out,
                 std::string& error);
bool build_clip(const tiled::Doc& d, const AseTag& tag, std::span<const AseFrame> frames,
                std::span<const AseSlice> slices, std::vector<ClipFrameSrc>& out, std::string& error);

} // namespace framework::graphics::clip_import
