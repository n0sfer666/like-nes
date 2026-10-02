#include "clip_parts.hpp"

#include <limits>

#include "sim_tick.hpp"

namespace framework::graphics::clip_import {

bool box_name(std::string_view name, BoxKind& kind, uint8_t& index) {
    if (name == "push") {
        kind = BoxKind::Push;
        index = 0;
        return true;
    }
    const std::string_view stem = name.substr(0, name.size() - (name.empty() ? 0 : 1));
    if (stem != "hit" && stem != "hurt") return false;
    const char digit = name.back();
    if (digit < '0' || digit >= static_cast<char>('0' + MAX_BOXES_OF_KIND)) return false;
    kind = stem == "hit" ? BoxKind::Hit : BoxKind::Hurt;
    index = static_cast<uint8_t>(digit - '0');
    return true;
}

bool word_ok(std::string_view name) {
    if (name.empty()) return false;
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

bool tag_ok(std::string_view tag) {
    return !tag.empty() && tag.find('/') == std::string_view::npos && tag.find('\0') == std::string_view::npos;
}

bool ticks_of_ms(int64_t ms, uint16_t& out) {
    if (ms <= 0 || ms > 1000000) return false;
    const uint64_t ticks = sim::ticks_from_ms(static_cast<uint64_t>(ms));
    if (ticks > std::numeric_limits<uint16_t>::max()) return false;
    out = static_cast<uint16_t>(ticks);
    return true;
}

void place(Pixel pivot, Pixel trim, ClipFrameSrc& frame) {
    frame.anchor_x = static_cast<int16_t>(pivot.x - trim.x);
    frame.anchor_y = static_cast<int16_t>(pivot.y - trim.y);
}

bool add_box(ClipFrameSrc& frame, BoxKind kind, uint8_t index, Pixel pivot, const Box& at) {
    for (const ClipBoxSrc& b : frame.boxes)
        if (b.kind == kind && b.index == index) return false;
    const Rect16 r{static_cast<int16_t>(at.x - pivot.x), static_cast<int16_t>(at.y - pivot.y),
                   static_cast<int16_t>(at.w), static_cast<int16_t>(at.h)};
    frame.boxes.push_back(ClipBoxSrc{kind, index, r});
    return true;
}

} // namespace framework::graphics::clip_import
