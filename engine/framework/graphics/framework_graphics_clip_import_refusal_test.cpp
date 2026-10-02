#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "clip_fixture.hpp"
#include "clip_import.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;

struct Bad {
    const char* what;
    const char* from;
    const char* to;
    const char* message;
};

bool edited(const std::string& text, const Bad& c, std::string& out) {
    const std::size_t at = text.find(c.from);
    if (at == std::string::npos) return false;
    out = text;
    out.replace(at, std::char_traits<char>::length(c.from), c.to);
    return true;
}

void report(bool ok, const std::string& error, const Bad& c, const char* where) {
    const bool named = error.find(c.message) != std::string::npos && error.rfind(where, 0) == 0;
    check(!ok && named, c.what);
    if (ok || !named) std::printf("    got: %s\n", error.c_str());
}

void refuse_json(const std::string& base, const char* file, std::span<const Bad> cases) {
    for (const Bad& c : cases) {
        std::string text;
        if (!edited(base, c, text)) {
            check(false, c.what);
            std::printf("    fixture has no '%s'\n", c.from);
            continue;
        }
        clip_fixture::FakeSource src = clip_fixture::source();
        std::vector<ClipSrc> out;
        std::string error;
        report(import_aseprite("brawler", file, std::as_bytes(std::span(text)), src, out, error), error, c, file);
    }
}

void test_aseprite() {
    const Bad brawler[] = {
        {"two tags of one name", "\"name\": \"back\"", "\"name\": \"idle\"", "a clip takes its name from its tag"},
        {"box past hit3", "\"name\": \"hit0\"", "\"name\": \"hit4\"", "is not a box; name boxes hit0..hit3"},
        {"repeat 3", "\"repeat\": \"1\"", "\"repeat\": \"3\"", "repeats a set number of times"},
        {"--ignore-empty gap", "brawler 3.aseprite", "brawler 4.aseprite", "--ignore-empty"},
        {"--split-tags repeat", "brawler 3.aseprite", "brawler 2.aseprite", "--split-tags"},
        {"--merge-duplicates cell", "\"x\": 96, \"y\": 0", "\"x\": 64, \"y\": 0", "--merge-duplicates"},
        {"box 1 px past the frame", "\"x\": 16, \"y\": 8, \"w\": 16", "\"x\": 16, \"y\": 8, \"w\": 17",
         "lies outside the 32x32 frame"},
        {"rotated frame", "\"rotated\": false", "\"rotated\": true", "rotated frame is not supported"},
        {"sheet of another size", "\"size\": { \"w\": 128", "\"size\": { \"w\": 120", "export the sheet and its JSON"},
        {"sheet without a texture record", "brawler.png", "brawlr.png", "is not a texture record"},
        {"zero duration", "\"duration\": 100", "\"duration\": 0", "'duration' must be 1 to 1000000 ms"},
        {"event past its tag", "\"1:swing\"", "\"3:swing\"", "lies outside tag 'punch'"},
        {"two events on a frame", "\"1:swing\"", "\"1:swing,1:duck\"", "two events on frame 1"},
        {"event without a frame", "\"1:swing\"", "\"swing\"", "<frame>:<event>"},
        {"no tags", "\"frameTags\"", "\"frameTagz\"", "export with --list-tags"},
        {"unknown direction", "\"direction\": \"reverse\"", "\"direction\": \"backwards\"", "pingpong_reverse"},
        {"tag past the frames", "\"to\": 3, \"direction\": \"pingpong\"", "\"to\": 4, \"direction\": \"pingpong\"",
         "spans frames 0 to 4"},
        {"keys out of order", "\"frame\": 3, \"bounds\": { \"x\": 10", "\"frame\": 1, \"bounds\": { \"x\": 10",
         "keys must go in frame order"},
        {"repeat 2 on a ping-pong", "\"pingpong_reverse\", \"repeat\": \"1\"",
         "\"pingpong_reverse\", \"repeat\": \"2\"", "repeats a set number of times"},
        {"first frame is not 0", "brawler 0.aseprite", "brawler 1.aseprite", "is not frame 0"},
        {"user data of another type", "\"data\": \"1:swing\"", "\"data\": 5", "'data'"},
        {"event grammar at the data", "\"1:swing\"", "\"swing\"", "brawler.json:20:14: tag 'punch' user data"},
        {"scaled sheet", "\"scale\": \"1\"", "\"scale\": \"2\"", "export without --scale"},
        {"slice declared twice", "\"name\": \"hit0\", \"color\"", "\"name\": \"hurt0\", \"color\"",
         "slice 'hurt0' is declared twice"},
        {"key past the frames", "{ \"frame\": 3, \"bounds\": { \"x\": 0", "{ \"frame\": 4, \"bounds\": { \"x\": 0",
         "has a key on frame 4 of 4"},
        {"box 1 px past the bottom", "\"x\": 16, \"y\": 8, \"w\": 16", "\"x\": 16, \"y\": 25, \"w\": 16",
         "lies outside the 32x32 frame"},
        {"broken JSON", "\"meta\"", "\"meta\" \"", ""},
    };
    refuse_json(clip_fixture::BRAWLER_JSON, "brawler.json", brawler);
    const Bad trim[] = {
        {"pivot outside the frame", "\"pivot\": { \"x\": 1", "\"pivot\": { \"x\": 60", "lies outside the frame"},
        {"trim of another size", "\"x\": 3, \"y\": 8, \"w\": 10", "\"x\": 3, \"y\": 8, \"w\": 11",
         "'spriteSourceSize' must be the size of 'frame'"},
        {"frames neither array nor hash", "\"frames\": {", "\"frames\": 7, \"x\": {", "json-array"},
    };
    refuse_json(clip_fixture::TRIM_JSON, "trim.json", trim);
}

void test_sheet() {
    const Bad cases[] = {
        {"unknown line", "grid|32|32\n", "grid|32|32\nframe|0\n", "brawler.sheet:4: unknown line 'frame'"},
        {"clip before the grid", "grid|32|32\n", "", "brawler.sheet:3: set 'image' and 'grid' before"},
        {"grid twice", "grid|32|32\n", "grid|32|32\ngrid|16|16\n", "brawler.sheet:4: the grid is set twice"},
        {"image twice", "grid|32|32\n", "image|brawler.png\n", "brawler.sheet:3: the image is set twice"},
        {"sheet without a texture record", "image|brawler.png", "image|brawlr.png", "is not a texture record"},
        {"box of an undeclared clip", "box|idle|0|", "box|walk|0|", "brawler.sheet:5: clip 'walk' is not declared"},
        {"box past the clip", "box|idle|1|", "box|idle|2|", "clip 'idle' has frames 0 to 1"},
        {"pivot is not a box", "box|punch|1|hit0", "box|punch|1|pivot", "box 'pivot' is unknown"},
        {"box past hit3", "box|punch|1|hit0", "box|punch|1|hit4", "box 'hit4' is unknown"},
        {"box of zero height", "box|punch|1|hit0|0|-24|16|8", "box|punch|1|hit0|0|-24|16|0", "a positive size"},
        {"box past the right of the cell", "box|punch|1|hit0|0|-24|16|8", "box|punch|1|hit0|1|-24|16|8",
         "leaves its 32x32 cell"},
        {"box below the cell", "box|punch|1|hit0|0|-24|16|8", "box|punch|1|hit0|0|-7|16|8", "leaves its 32x32 cell"},
        {"box twice", "box|idle|0|hurt0|-8|-28|16|28\n",
         "box|idle|0|hurt0|-8|-28|16|28\nbox|idle|0|hurt0|0|-1|1|1\n", "is set twice on this frame"},
        {"clip past the sheet", "row=0|from=0|to=3|ticks=6|loop", "row=0|from=0|to=4|ticks=6|loop",
         "reaches past the 128x32 sheet"},
        {"row past the sheet", "clip|idle|row=0", "clip|idle|row=1", "reaches past the 128x32 sheet"},
        {"zero ticks", "to=1|ticks=6", "to=1|ticks=0", "ticks= takes 1 to 65535"},
        {"unknown option", "|loop|pingpong", "|loop|bounce", "'bounce' is unknown"},
        {"repeated option", "clip|idle|row=0|", "clip|idle|row=0|row=0|", "'row=0' is unknown, repeated"},
        {"missing ticks", "to=1|ticks=6|loop", "to=1|loop|pingpong", "needs row=, from=, to= and ticks="},
        {"two clips of one name", "clip|back|", "clip|idle|", "needs a name of its own"},
        {"event of other characters", "event|punch|1|swing", "event|punch|1|sw ing", "must be letters"},
        {"two events on a frame", "event|punch|1|swing\n", "event|punch|1|swing\nevent|punch|1|duck\n",
         "already has event 'swing'"},
    };
    for (const Bad& c : cases) {
        std::string text;
        if (!edited(clip_fixture::BRAWLER_SHEET, c, text)) {
            check(false, c.what);
            continue;
        }
        clip_fixture::FakeSource src = clip_fixture::source();
        std::vector<ClipSrc> out;
        std::string error;
        report(import_sheet("brawler", "brawler.sheet", text, src, out, error), error, c, "brawler.sheet:");
    }
    clip_fixture::FakeSource src = clip_fixture::source();
    std::vector<ClipSrc> out;
    std::string error;
    const Bad empty{"sheet without clips", "", "", "the sheet declares no clip"};
    report(import_sheet("brawler", "brawler.sheet", "image|brawler.png\ngrid|32|32\n", src, out, error), error, empty,
           "brawler.sheet:");
}

} // namespace

int main() {
    test_aseprite();
    test_sheet();
    std::printf("framework-graphics-clip-import-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
