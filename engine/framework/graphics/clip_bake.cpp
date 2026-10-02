#include "clip_bake.hpp"

#include <algorithm>
#include <limits>

#include "clip_read.hpp"
#include "section_bake.hpp"

namespace framework::graphics {
namespace {

constexpr std::size_t MAX_FRAMES = std::numeric_limits<uint16_t>::max();
constexpr std::size_t MAX_BOXES = std::numeric_limits<uint16_t>::max();

bool refuse(std::string& error, const std::string& message) {
    error = message;
    return false;
}

bool box_ok(const ClipFrameSrc& f, std::size_t i, const std::string& who, std::string& error) {
    const ClipBoxSrc& b = f.boxes[i];
    const uint8_t kind = static_cast<uint8_t>(b.kind);
    if (kind > static_cast<uint8_t>(BoxKind::Push) || b.index >= box_limit(kind))
        return refuse(error, who + " has a box of an unknown kind or index");
    if (!clip_rect_ok(b.rect))
        return refuse(error, who + " has a box with a zero or negative side, or one that ends past 32767");
    for (std::size_t j = 0; j < i; ++j)
        if (f.boxes[j].kind == b.kind && f.boxes[j].index == b.index)
            return refuse(error, who + " has the same box twice");
    return true;
}

bool check_clip(const ClipSrc& c, std::string& error) {
    if (c.frames.empty() || c.frames.size() > MAX_FRAMES)
        return refuse(error, "clip '" + c.name + "' has " + std::to_string(c.frames.size()) +
                                 " frames; allowed 1 to 65535");
    if ((c.flags & ~CLIP_FLAGS_ALL) != 0) return refuse(error, "clip '" + c.name + "' has unknown flags");
    std::size_t boxes = 0;
    std::vector<ClipFrame> timing;
    for (std::size_t i = 0; i < c.frames.size(); ++i) {
        const ClipFrameSrc& f = c.frames[i];
        const std::string who = "frame " + std::to_string(i) + " of clip '" + c.name + "'";
        if (f.w == 0 || f.h == 0) return refuse(error, who + " has a zero side");
        if (f.duration == 0) return refuse(error, who + " lasts zero ticks");
        for (std::size_t b = 0; b < f.boxes.size(); ++b)
            if (!box_ok(f, b, who, error)) return false;
        boxes += f.boxes.size();
        timing.push_back(ClipFrame{0, f.duration, ANIM_EVENT_NONE});
    }
    if (boxes > MAX_BOXES) return refuse(error, "clip '" + c.name + "' has more than 65535 boxes");
    if (clip_period_wide(timing, c.flags) > std::numeric_limits<uint32_t>::max())
        return refuse(error, "clip '" + c.name + "' lasts more than 4294967295 ticks a period");
    return true;
}

bool box_before(const ClipBoxSrc& a, const ClipBoxSrc& b) {
    return a.kind < b.kind || (a.kind == b.kind && a.index < b.index);
}

AnimEvent event_of(core::SectionBuilder& b, const std::string& name, std::vector<std::string>& names,
                   std::vector<uint32_t>& offsets) {
    if (name.empty()) return ANIM_EVENT_NONE;
    const auto it = std::find(names.begin(), names.end(), name);
    if (it != names.end()) return static_cast<AnimEvent>(it - names.begin() + 1);
    names.push_back(name);
    offsets.push_back(b.text(name));
    return static_cast<AnimEvent>(names.size());
}

void clip_of(core::SectionBuilder& b, const ClipSrc& c, ClipRow& row) {
    std::vector<ClipFrame> frames;
    std::vector<ClipCel> cels;
    std::vector<ClipBox> boxes;
    std::vector<std::string> names;
    std::vector<uint32_t> events;
    for (std::size_t i = 0; i < c.frames.size(); ++i) {
        const ClipFrameSrc& f = c.frames[i];
        std::vector<ClipBoxSrc> sorted = f.boxes;
        std::sort(sorted.begin(), sorted.end(), box_before);
        cels.push_back(ClipCel{f.x, f.y, f.w, f.h, f.anchor_x, f.anchor_y, static_cast<uint16_t>(boxes.size()),
                               static_cast<uint16_t>(sorted.size())});
        for (const ClipBoxSrc& s : sorted) boxes.push_back(ClipBox{static_cast<uint8_t>(s.kind), s.index, 0, s.rect});
        frames.push_back(ClipFrame{static_cast<RegionId>(i), f.duration, event_of(b, f.event, names, events)});
    }
    row.name_offset = b.text(c.name);
    row.flags = c.flags;
    row.frame_count = static_cast<uint16_t>(frames.size());
    row.texture_guid = c.texture_guid;
    row.frame_offset = b.block(frames.data(), frames.size() * sizeof(ClipFrame), alignof(ClipFrame));
    row.cel_offset = b.block(cels.data(), cels.size() * sizeof(ClipCel), alignof(ClipCel));
    row.box_offset = b.block(boxes.data(), boxes.size() * sizeof(ClipBox), alignof(ClipBox));
    row.box_count = static_cast<uint32_t>(boxes.size());
    row.event_offset = b.block(events.data(), events.size() * sizeof(uint32_t), alignof(uint32_t));
    row.event_count = static_cast<uint32_t>(events.size());
}

} // namespace

bool check_clips(std::span<const ClipSrc> clips, std::string& error) {
    if (clips.empty()) return refuse(error, "no clip to bake");
    for (std::size_t i = 0; i < clips.size(); ++i) {
        if (clips[i].name.empty()) return refuse(error, "a clip needs a name");
        for (std::size_t j = 0; j < i; ++j)
            if (clips[j].name == clips[i].name)
                return refuse(error, "clip '" + clips[i].name + "' is declared twice");
        if (!check_clip(clips[i], error)) return false;
    }
    return true;
}

bool bake_clips(std::span<const ClipSrc> clips, std::vector<uint8_t>& out, std::string& error) {
    if (!check_clips(clips, error)) return false;
    std::vector<ClipRow> rows(clips.size(), ClipRow{});
    core::SectionBuilder b(rows.size() * sizeof(ClipRow));
    for (std::size_t i = 0; i < clips.size(); ++i) clip_of(b, clips[i], rows[i]);
    if (!b.finish(CLIP_MAGIC, CLIP_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), out, error))
        return false;
    ClipTable probe;
    if (!probe.open(out.data(), out.size())) return refuse(error, "baked clip table fails its own reader");
    return true;
}

} // namespace framework::graphics
