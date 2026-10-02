#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "clip_format.hpp"

namespace framework::graphics {

struct ClipBoxSrc {
    BoxKind kind = BoxKind::Hit;
    uint8_t index = 0;
    Rect16 rect{};
};

struct ClipFrameSrc {
    uint16_t x = 0, y = 0, w = 0, h = 0;
    int16_t anchor_x = 0, anchor_y = 0;
    uint16_t duration = 1;
    std::string event;
    std::vector<ClipBoxSrc> boxes;
};

struct ClipSrc {
    std::string name;
    uint16_t flags = CLIP_ONCE;
    uint64_t texture_guid = 0;
    std::vector<ClipFrameSrc> frames;
};

bool check_clips(std::span<const ClipSrc> clips, std::string& error);
bool bake_clips(std::span<const ClipSrc> clips, std::vector<uint8_t>& out, std::string& error);

} // namespace framework::graphics
