#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "clip_format.hpp"
#include "section_open.hpp"

namespace framework::graphics {

struct ClipView {
    const ClipRow* row = nullptr;
    Clip clip;
    std::span<const ClipCel> cels;
    std::span<const ClipBox> boxes;
    std::span<const uint32_t> events;
    const char* strings = nullptr;
};

class ClipTable {
public:
    bool open(const void* data, std::size_t size);
    bool valid() const { return view_.header != nullptr; }
    uint32_t count() const { return valid() ? view_.header->count : 0; }
    const char* name(uint32_t index) const;
    ClipView clip(uint32_t index) const;
    ClipView find(const char* name) const;

private:
    core::SectionView view_;
    const ClipRow* rows_ = nullptr;
};

bool clip_rect_ok(const Rect16& r);
uint64_t clip_period_wide(std::span<const ClipFrame> frames, uint16_t flags);
std::span<const ClipBox> cel_boxes(const ClipView& view, const ClipCel& cel);
const ClipCel* frame_cel(const ClipView& view, uint16_t frame);
std::span<const ClipBox> frame_boxes(const ClipView& view, uint16_t frame, BoxKind kind);
const char* clip_event_name(const ClipView& view, AnimEvent event);

} // namespace framework::graphics
