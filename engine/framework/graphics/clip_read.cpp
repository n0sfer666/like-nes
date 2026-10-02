#include "clip_read.hpp"

#include <cstring>
#include <limits>

namespace framework::graphics {
namespace {

bool box_ok(const ClipBox& b) {
    return b.kind <= static_cast<uint8_t>(BoxKind::Push) && b.index < box_limit(b.kind) && b.pad == 0 &&
           clip_rect_ok(b.rect);
}

bool box_before(const ClipBox& a, const ClipBox& b) {
    return a.kind < b.kind || (a.kind == b.kind && a.index < b.index);
}

bool cel_ok(const ClipCel& c, std::span<const ClipBox> boxes) {
    if (c.w == 0 || c.h == 0 || uint32_t{c.first_box} + c.box_count > boxes.size()) return false;
    const std::span<const ClipBox> own = boxes.subspan(c.first_box, c.box_count);
    for (std::size_t i = 0; i < own.size(); ++i) {
        if (!box_ok(own[i])) return false;
        if (i > 0 && !box_before(own[i - 1], own[i])) return false;
    }
    return true;
}

bool after(uint32_t offset, std::size_t bytes, uint64_t& cursor) {
    if (offset < cursor) return false;
    cursor = uint64_t{offset} + bytes;
    return true;
}

bool row_ok(const core::SectionView& v, const ClipRow& r, uint64_t& cursor) {
    std::span<const ClipFrame> frames;
    std::span<const ClipCel> cels;
    std::span<const ClipBox> boxes;
    std::span<const uint32_t> events;
    if (!v.text(r.name_offset) || (r.flags & ~CLIP_FLAGS_ALL) != 0 || r.frame_count == 0 ||
        r.event_count > r.frame_count)
        return false;
    if (!v.take(r.frame_offset, r.frame_count, frames) || !v.take(r.cel_offset, r.frame_count, cels) ||
        !v.take(r.box_offset, r.box_count, boxes) || !v.take(r.event_offset, r.event_count, events))
        return false;
    if (!after(r.frame_offset, frames.size_bytes(), cursor) || !after(r.cel_offset, cels.size_bytes(), cursor) ||
        !after(r.box_offset, boxes.size_bytes(), cursor) || !after(r.event_offset, events.size_bytes(), cursor))
        return false;
    for (const uint32_t e : events)
        if (!v.text(e)) return false;
    for (const ClipFrame& f : frames)
        if (f.region >= r.frame_count || f.duration == 0 || f.event > r.event_count) return false;
    if (clip_period_wide(frames, r.flags) > std::numeric_limits<uint32_t>::max()) return false;
    for (const ClipCel& c : cels)
        if (!cel_ok(c, boxes)) return false;
    return true;
}

} // namespace

bool ClipTable::open(const void* data, std::size_t size) {
    view_ = core::SectionView{};
    rows_ = nullptr;
    core::SectionView v;
    if (!core::open_section(data, size, CLIP_MAGIC, CLIP_VERSION, sizeof(ClipRow), alignof(ClipRow), v))
        return false;
    const std::span<const ClipRow> rows = v.at<ClipRow>(v.header->rows_offset, v.header->count);
    uint64_t cursor = v.rows_end;
    for (const ClipRow& r : rows)
        if (!row_ok(v, r, cursor)) return false;
    view_ = v;
    rows_ = rows.data();
    return true;
}

const char* ClipTable::name(uint32_t index) const {
    if (index >= count()) return "";
    return view_.strings + rows_[index].name_offset;
}

ClipView ClipTable::clip(uint32_t index) const {
    if (index >= count()) return {};
    const ClipRow& r = rows_[index];
    ClipView c;
    c.row = &r;
    c.clip = Clip{view_.at<ClipFrame>(r.frame_offset, r.frame_count).data(), r.frame_count, r.flags};
    c.cels = view_.at<ClipCel>(r.cel_offset, r.frame_count);
    c.boxes = view_.at<ClipBox>(r.box_offset, r.box_count);
    c.events = view_.at<uint32_t>(r.event_offset, r.event_count);
    c.strings = view_.strings;
    return c;
}

ClipView ClipTable::find(const char* wanted) const {
    if (wanted == nullptr) return {};
    for (uint32_t i = 0; i < count(); ++i)
        if (std::strcmp(name(i), wanted) == 0) return clip(i);
    return {};
}

bool clip_rect_ok(const Rect16& r) {
    constexpr int32_t far = std::numeric_limits<int16_t>::max();
    return r.w > 0 && r.h > 0 && int32_t{r.x} + r.w <= far && int32_t{r.y} + r.h <= far;
}

uint64_t clip_period_wide(std::span<const ClipFrame> frames, uint16_t flags) {
    uint64_t total = 0;
    for (const ClipFrame& f : frames) total += f.duration;
    if ((flags & CLIP_PINGPONG) == 0 || frames.size() < 2) return total;
    return 2 * total - frames.front().duration - frames.back().duration;
}

std::span<const ClipBox> cel_boxes(const ClipView& view, const ClipCel& cel) {
    if (std::size_t{cel.first_box} + cel.box_count > view.boxes.size()) return {};
    return view.boxes.subspan(cel.first_box, cel.box_count);
}

const char* clip_event_name(const ClipView& view, AnimEvent event) {
    if (event == ANIM_EVENT_NONE || event > view.events.size() || view.strings == nullptr) return "";
    return view.strings + view.events[event - 1u];
}

} // namespace framework::graphics
