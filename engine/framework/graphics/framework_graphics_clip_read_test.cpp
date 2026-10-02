#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "clip_bake.hpp"
#include "clip_fixture.hpp"
#include "clip_read.hpp"
#include "section_format.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

std::vector<uint8_t> bake(const std::vector<ClipSrc>& clips) {
    std::vector<uint8_t> out;
    std::string error;
    if (!bake_clips(clips, out, error)) std::printf("    bake: %s\n", error.c_str());
    return out;
}

std::vector<uint8_t> u32(uint32_t v) {
    std::vector<uint8_t> b(4);
    std::memcpy(b.data(), &v, 4);
    return b;
}

std::vector<uint8_t> u16(uint16_t v) {
    std::vector<uint8_t> b(2);
    std::memcpy(b.data(), &v, 2);
    return b;
}

void refused(const std::vector<uint8_t>& bytes, const char* what, const void* field, std::vector<uint8_t> patch) {
    std::vector<uint8_t> bad = bytes;
    const auto offset = static_cast<std::size_t>(static_cast<const uint8_t*>(field) - bytes.data());
    std::memcpy(bad.data() + offset, patch.data(), patch.size());
    ClipTable r;
    check(!r.open(bad.data(), bad.size()), what);
}

void test_corruptions() {
    const std::vector<uint8_t> bytes = bake(clip_fixture::clips());
    ClipTable t;
    const bool opened = t.open(bytes.data(), bytes.size());
    const ClipView run = t.find("hero/run");
    const ClipView jab = t.find("hero/jab");
    check(opened && run.row != nullptr && jab.row != nullptr && run.boxes.size() == 4, "fixture table opens");
    if (run.row == nullptr || jab.row == nullptr || run.boxes.size() != 4) return;
    framework::core::SectionHeader head{};
    std::memcpy(&head, bytes.data(), sizeof(head));
    refused(bytes, "wrong magic", bytes.data(), {'L', 'N', 'C', 'X'});
    refused(bytes, "unknown flags", &run.row->flags, u16(4));
    refused(bytes, "zero frames", &run.row->frame_count, u16(0));
    refused(bytes, "name past the strings", &run.row->name_offset, u32(0xFFFF));
    refused(bytes, "frames inside the rows", &run.row->frame_offset, u32(head.rows_offset));
    refused(bytes, "cels at the strings", &run.row->cel_offset, u32(head.strings_offset));
    refused(bytes, "misaligned boxes", &run.row->box_offset, u32(run.row->box_offset + 1));
    refused(bytes, "more events than frames", &run.row->event_count, u32(4));
    refused(bytes, "event block past the strings", &jab.row->event_count, u32(2));
    refused(bytes, "row shares frames of the row before", &jab.row->frame_offset, u32(run.row->frame_offset));
    refused(bytes, "region past the frames", &run.clip.frames[0].region, u16(3));
    refused(bytes, "frame of zero ticks", &run.clip.frames[1].duration, u16(0));
    refused(bytes, "event past the table", &run.clip.frames[1].event, u16(2));
    refused(bytes, "cel of zero width", &run.cels[1].w, u16(0));
    refused(bytes, "cel boxes past the block", &run.cels[2].box_count, u16(2));
    refused(bytes, "unknown box kind", &run.boxes[0].kind, {3});
    refused(bytes, "push index 1", &run.boxes[2].index, {1});
    refused(bytes, "box padding set", &run.boxes[0].pad, {1, 0});
    refused(bytes, "boxes out of order", &run.boxes[1].index, {0});
    refused(bytes, "box of zero height", &run.boxes[3].rect.h, u16(0));
    refused(bytes, "box past 32767", &run.boxes[3].rect.x, u16(32760));
    refused(bytes, "event name past the strings", &run.events[0], u32(0xFFFF));
    std::vector<uint8_t> cut(bytes.begin(), bytes.end() - 8);
    ClipTable r;
    check(!r.open(cut.data(), cut.size()), "truncated section");
}

void test_long_period() {
    std::vector<ClipSrc> clips = clip_fixture::clips();
    clips[1].frames.assign(65535, ClipFrameSrc{0, 0, 1, 1, 0, 0, 65535, "", {}});
    clips[1].flags = CLIP_LOOP;
    const std::vector<uint8_t> bytes = bake(clips);
    ClipTable t;
    check(t.open(bytes.data(), bytes.size()), "65535 frames of 65535 ticks fit 32 bits once through");
    const ClipView jab = t.find("hero/jab");
    if (jab.row == nullptr) return;
    refused(bytes, "ping-pong doubles the period past 32 bits", &jab.row->flags, u16(CLIP_LOOP | CLIP_PINGPONG));
}

void test_cel_boxes() {
    const std::vector<uint8_t> bytes = bake(clip_fixture::clips());
    ClipTable t;
    t.open(bytes.data(), bytes.size());
    const ClipView run = t.find("hero/run");
    check(cel_boxes(run, ClipCel{0, 0, 1, 1, 0, 0, 3, 2}).empty(), "cel_boxes past the block is empty");
    check(cel_boxes(ClipView{}, ClipCel{}).empty(), "cel_boxes of no clip is empty");
}

} // namespace

int main() {
    test_corruptions();
    test_long_period();
    test_cel_boxes();
    std::printf("framework-graphics-clip-read: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
