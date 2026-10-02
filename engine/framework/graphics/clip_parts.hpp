#pragma once
#include <cstdint>
#include <string>
#include <string_view>

#include "clip_bake.hpp"

namespace framework::graphics::clip_import {

constexpr int64_t MAX_SIDE = 32767;
constexpr const char* BOX_NAMES = "hit0..hit3, hurt0..hurt3 or push";

struct Pixel {
    int64_t x = 0, y = 0;
};

struct Box {
    int64_t x = 0, y = 0, w = 0, h = 0;
};

bool box_name(std::string_view name, BoxKind& kind, uint8_t& index);
bool word_ok(std::string_view name);
bool tag_ok(std::string_view tag);
bool ticks_of_ms(int64_t ms, uint16_t& out);
void place(Pixel pivot, Pixel trim, ClipFrameSrc& frame);
bool add_box(ClipFrameSrc& frame, BoxKind kind, uint8_t index, Pixel pivot, const Box& at);

} // namespace framework::graphics::clip_import
