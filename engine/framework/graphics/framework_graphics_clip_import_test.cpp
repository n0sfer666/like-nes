#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "clip_fixture.hpp"
#include "clip_import.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

const char* DEFAULT_QUEEN = "games/neon-rumble/assets/chewbatrij/queen.json";

std::span<const std::byte> bytes_of(const std::string& s) { return std::as_bytes(std::span(s)); }

std::vector<uint8_t> baked(const std::vector<ClipSrc>& clips) {
    std::vector<uint8_t> out;
    std::string error;
    if (!bake_clips(clips, out, error)) std::printf("    bake: %s\n", error.c_str());
    return out;
}

bool rect_is(const ClipBoxSrc& b, BoxKind kind, int16_t x, int16_t y, int16_t w, int16_t h) {
    return b.kind == kind && b.index == 0 && b.rect.x == x && b.rect.y == y && b.rect.w == w && b.rect.h == h;
}

void test_brawler() {
    clip_fixture::FakeSource src = clip_fixture::source();
    std::vector<ClipSrc> ase;
    std::vector<ClipSrc> sheet;
    std::string error;
    check(import_aseprite("brawler", "brawler.json", bytes_of(clip_fixture::BRAWLER_JSON), src, ase, error),
          "brawler JSON imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    check(import_sheet("brawler", "brawler.sheet", clip_fixture::BRAWLER_SHEET, src, sheet, error),
          "brawler sheet imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    if (ase.size() != 5) {
        check(false, "a clip per tag");
        return;
    }
    check(ase[0].name == "brawler/idle" && ase[4].name == "brawler/bobr", "clip name is record/tag");
    check(ase[0].texture_guid == 0xB1, "texture guid of the sheet beside the JSON");
    check(ase[0].flags == CLIP_LOOP && ase[1].flags == CLIP_ONCE && ase[2].flags == CLIP_LOOP &&
              ase[3].flags == (CLIP_LOOP | CLIP_PINGPONG) && ase[4].flags == CLIP_ONCE,
          "direction and repeat to flags; a ping-pong of repeat 1 is one pass");
    check(ase[0].frames[0].duration == 6 && ase[0].frames[0].anchor_x == 16 && ase[0].frames[0].anchor_y == 32,
          "100 ms is 6 ticks, default pivot at the bottom centre");
    const std::vector<ClipFrameSrc>& back = ase[2].frames;
    check(back.size() == 3 && back[0].x == 64 && back[1].x == 32 && back[2].x == 0, "reverse plays back to front");
    const std::vector<ClipFrameSrc>& punch = ase[1].frames;
    check(punch[0].boxes.empty(), "a key before the tag does not reach into it");
    check(punch[1].boxes.size() == 1 && rect_is(punch[1].boxes[0], BoxKind::Hit, 0, -24, 16, 8),
          "hit box flush with the right edge, relative to the pivot");
    check(punch[2].boxes.size() == 1 && rect_is(punch[2].boxes[0], BoxKind::Hurt, -6, -28, 16, 28),
          "a 0x0 key switches a box off, the next key brings it back");
    check(punch[1].event == "swing" && punch[0].event.empty(), "event from tag user data");
    check(ase[4].frames[3].event == "step" && ase[4].frames[3].x == 0, "event follows its frame through reversal");
    check(baked(ase) == baked(sheet), "Aseprite JSON and .sheet bake to the same bytes");
}

void test_trim() {
    clip_fixture::FakeSource src = clip_fixture::source();
    std::vector<ClipSrc> out;
    std::string error;
    check(import_aseprite("trim", "trim.json", bytes_of(clip_fixture::TRIM_JSON), src, out, error),
          "trimmed hash JSON imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    if (out.size() != 2 || out[0].frames.size() != 2 || out[1].frames.size() != 1) {
        check(false, "two tags of the trimmed sheet");
        return;
    }
    const ClipFrameSrc& f0 = out[0].frames[0];
    const ClipFrameSrc& f1 = out[0].frames[1];
    check(f0.w == 10 && f0.h == 12 && f0.anchor_x == 4 && f0.anchor_y == 3, "anchor is the pivot minus the trim");
    check(f1.anchor_x == 7 && f1.anchor_y == 11 && f1.duration == 15, "pivot key carries on, 250 ms is 15 ticks");
    check(f0.boxes.size() == 1 && rect_is(f0.boxes[0], BoxKind::Hit, 0, 0, 9, 2), "box at the pivot of a trimmed cel");
    const ClipFrameSrc& late = out[1].frames[0];
    check(late.anchor_x == 8 && late.anchor_y == 20 && late.boxes.empty(), "keys before the tag stay out of it");
}

std::string with(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    if (at != std::string::npos) text.replace(at, from.size(), to);
    return text;
}

void test_durations() {
    std::string json = clip_fixture::BRAWLER_JSON;
    for (const char* ms : {"25", "1", "8"}) json = with(json, "\"duration\": 100", std::string("\"duration\": ") + ms);
    clip_fixture::FakeSource src = clip_fixture::source();
    std::vector<ClipSrc> out;
    std::string error;
    check(import_aseprite("brawler", "brawler.json", bytes_of(json), src, out, error), "short durations import");
    if (out.size() != 5) return;
    check(out[0].frames[0].duration == 2 && out[0].frames[1].duration == 1 && out[2].frames[0].duration == 1,
          "25 ms is 2 ticks, 1 ms and 8 ms round up to 1");
}

void test_bare_pivot() {
    std::string json = clip_fixture::TRIM_JSON;
    const std::size_t from = json.find("\"w\": 2, \"h\": 2 }");
    const std::size_t to = json.find("\"y\": 1 } }", from);
    if (from == std::string::npos || to == std::string::npos) {
        check(false, "trim fixture has the pivot key");
        return;
    }
    json.replace(from, to + 10 - from, "\"w\": 4, \"h\": 6 } }");
    clip_fixture::FakeSource src = clip_fixture::source();
    std::vector<ClipSrc> out;
    std::string error;
    check(import_aseprite("trim", "trim.json", bytes_of(json), src, out, error), "pivot slice without 'pivot'");
    if (out.empty()) return;
    const ClipFrameSrc& f0 = out[0].frames[0];
    check(f0.anchor_x == 5 && f0.anchor_y == 5 && f0.boxes.size() == 1 &&
              rect_is(f0.boxes[0], BoxKind::Hit, -1, -2, 9, 2),
          "a pivot slice without 'pivot' pins its centre");
}

bool png_size(const std::vector<uint8_t>& png, uint32_t& w, uint32_t& h) {
    if (png.size() < 24) return false;
    const auto be = [&](std::size_t at) {
        return (uint32_t{png[at]} << 24) | (uint32_t{png[at + 1]} << 16) | (uint32_t{png[at + 2]} << 8) | png[at + 3];
    };
    w = be(16);
    h = be(20);
    return true;
}

void test_queen(const std::string& json_path) {
    std::vector<uint8_t> json;
    std::vector<uint8_t> png;
    const std::string png_path = json_path.substr(0, json_path.size() - 4) + "png";
    framework::tiled::Image image{0x51, 0, 0};
    if (!platform::read_bytes(json_path, json) || !platform::read_bytes(png_path, png) ||
        !png_size(png, image.width, image.height)) {
        check(false, "queen.json and queen.png read");
        return;
    }
    clip_fixture::FakeSource src;
    src.images = {{"queen.png", image}};
    std::vector<ClipSrc> ase;
    std::vector<ClipSrc> sheet;
    std::string error;
    check(import_aseprite("queen", json_path, std::as_bytes(std::span(json)), src, ase, error),
          "real Aseprite 1.2.8 export with hash frames imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    check(import_sheet("queen", "queen.sheet", clip_fixture::QUEEN_SHEET, src, sheet, error), "queen sheet imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    check(ase.size() == 20, "twenty tags, twenty clips");
    if (ase.size() != 20) return;
    check(ase[1].name == "queen/Death" && ase[1].frames.size() == 6 && ase[2].frames.size() == 1,
          "overlapping tags each own their frames");
    check(ase[9].name == "queen/Walk+Knuckles" && ase[9].frames[0].x == 15 * 74, "frames in key order of the hash");
    check(baked(ase) == baked(sheet), "queen JSON and .sheet bake to the same bytes");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    test_brawler();
    test_trim();
    test_durations();
    test_bare_pivot();
    test_queen(argc > 1 ? argv[1] : DEFAULT_QUEEN);
    std::printf("framework-graphics-clip-import: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
