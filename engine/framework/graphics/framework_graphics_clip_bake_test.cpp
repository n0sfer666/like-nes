#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "clip_bake.hpp"
#include "clip_fixture.hpp"
#include "clip_read.hpp"
#include "hash_mix.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

constexpr uint64_t GOLDEN = 0xcfd3a526e9ff27ebull;

uint64_t hash_bytes(const std::vector<uint8_t>& b) {
    uint64_t h = framework::physics::FNV_OFFSET;
    framework::physics::mix_bytes(h, b.data(), b.size());
    return h;
}

bool same(const Rect16& a, int16_t x, int16_t y, int16_t w, int16_t h) {
    return a.x == x && a.y == y && a.w == w && a.h == h;
}

void test_round_trip(const std::vector<uint8_t>& bytes) {
    ClipTable t;
    check(t.open(bytes.data(), bytes.size()), "baked table opens");
    check(t.count() == 2, "two clips");
    check(std::strcmp(t.name(0), "hero/run") == 0 && std::strcmp(t.name(1), "hero/jab") == 0, "names in order");
    const ClipView run = t.find("hero/run");
    check(run.row != nullptr && run.row->texture_guid == 0xA1, "texture guid on the row");
    if (run.row == nullptr) return;
    check(run.clip.frame_count == 3 && run.clip.flags == CLIP_LOOP, "run: frames and flags");
    check(run.clip.frames[0].region == 0 && run.clip.frames[2].region == 2, "region is the frame index");
    check(run.clip.frames[1].duration == 4, "duration in ticks");
    check(run.events.size() == 1 && run.clip.frames[0].event == 1 && run.clip.frames[1].event == ANIM_EVENT_NONE &&
              run.clip.frames[2].event == 1,
          "one event name shared by two frames");
    check(std::strcmp(clip_event_name(run, 1), "step") == 0, "event name");
    check(std::strcmp(clip_event_name(run, 2), "") == 0 && std::strcmp(clip_event_name(run, 0), "") == 0,
          "no name past the table or for none");
    const ClipCel& c0 = run.cels[0];
    check(c0.x == 0 && c0.w == 16 && c0.h == 24 && c0.anchor_x == 8 && c0.anchor_y == 24, "cel rect and anchor");
    const std::span<const ClipBox> b0 = cel_boxes(run, c0);
    check(b0.size() == 3, "three boxes on frame 0");
    if (b0.size() == 3)
        check(b0[0].kind == 1 && b0[0].index == 0 && b0[1].kind == 1 && b0[1].index == 1 && b0[2].kind == 2 &&
                  same(b0[0].rect, -6, -16, 12, 16),
              "boxes sorted by kind then index");
    check(cel_boxes(run, run.cels[1]).empty(), "frame without boxes");
    const std::span<const ClipBox> b2 = cel_boxes(run, run.cels[2]);
    check(b2.size() == 1 && b2[0].kind == 0 && same(b2[0].rect, 2, -18, 10, 6), "hit box keeps its rect");
    const ClipView jab = t.find("hero/jab");
    check(jab.row != nullptr && jab.clip.flags == CLIP_PINGPONG && jab.boxes.size() == 1, "jab: own box block");
    if (jab.row != nullptr)
        check(jab.cels[1].anchor_x == -3 && jab.cels[1].anchor_y == 30 && cel_boxes(jab, jab.cels[1])[0].index == 3,
              "anchor may leave the cel");
    check(t.find("hero/idle").row == nullptr && t.find(nullptr).row == nullptr, "unknown name finds nothing");
}

struct Bad {
    const char* what;
    std::function<void(std::vector<ClipSrc>&)> edit;
    const char* message;
};

void test_bake_refusals() {
    const Bad cases[] = {
        {"no clip", [](auto& c) { c.clear(); }, "no clip to bake"},
        {"unnamed clip", [](auto& c) { c[1].name.clear(); }, "needs a name"},
        {"duplicate clip", [](auto& c) { c[1].name = "hero/run"; }, "declared twice"},
        {"clip without frames", [](auto& c) { c[1].frames.clear(); }, "allowed 1 to 65535"},
        {"unknown flags", [](auto& c) { c[0].flags = 4; }, "unknown flags"},
        {"zero side", [](auto& c) { c[0].frames[1].h = 0; }, "zero side"},
        {"zero duration", [](auto& c) { c[0].frames[1].duration = 0; }, "zero ticks"},
        {"push index 1", [](auto& c) { c[0].frames[0].boxes[0].index = 1; }, "unknown kind or index"},
        {"hit index 4", [](auto& c) { c[0].frames[2].boxes[0].index = 4; }, "unknown kind or index"},
        {"negative box side", [](auto& c) { c[0].frames[2].boxes[0].rect.w = -1; }, "zero or negative side"},
        {"same box twice", [](auto& c) { c[0].frames[0].boxes[1].index = 0; }, "same box twice"},
        {"box past 32767", [](auto& c) { c[0].frames[2].boxes[0].rect.x = 32760; }, "ends past 32767"},
        {"period past 32 bits",
         [](auto& c) {
             c[1].flags = CLIP_LOOP | CLIP_PINGPONG;
             c[1].frames.assign(65535, ClipFrameSrc{0, 0, 1, 1, 0, 0, 65535, "", {}});
         },
         "lasts more than 4294967295 ticks"},
    };
    for (const Bad& c : cases) {
        std::vector<ClipSrc> src = clip_fixture::clips();
        c.edit(src);
        std::vector<uint8_t> out;
        std::string error;
        const bool ok = bake_clips(src, out, error);
        check(!ok && error.find(c.message) != std::string::npos, c.what);
        if (ok || error.find(c.message) == std::string::npos) std::printf("    got: %s\n", error.c_str());
    }
}

} // namespace

int main() {
    std::vector<uint8_t> bytes;
    std::string error;
    check(bake_clips(clip_fixture::clips(), bytes, error), "clips bake");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    test_round_trip(bytes);
    const uint64_t h = hash_bytes(bytes);
    check(h == GOLDEN, "byte table matches the golden");
    if (h != GOLDEN) std::printf("    hash 0x%016llx\n", static_cast<unsigned long long>(h));
    test_bake_refusals();
    std::printf("framework-graphics-clip-bake: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
