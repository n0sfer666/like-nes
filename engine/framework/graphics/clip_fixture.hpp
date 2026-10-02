#pragma once

#include <string>
#include <utility>
#include <vector>

#include "clip_bake.hpp"
#include "tiled_import.hpp"

namespace clip_fixture {

class FakeSource : public framework::tiled::Source {
public:
    std::vector<std::pair<std::string, framework::tiled::Image>> images;

    bool tileset(const std::string&, const std::string& rel, std::string&, std::vector<uint8_t>&,
                 std::string& error) override {
        error = "no tileset '" + rel + "'";
        return false;
    }

    bool image(const std::string&, const std::string& rel, framework::tiled::Image& out, std::string& error) override {
        for (const auto& [name, image] : images)
            if (name == rel) {
                out = image;
                return true;
            }
        error = "image '" + rel + "' is not a texture record";
        return false;
    }
};

inline std::vector<framework::graphics::ClipSrc> clips() {
    using namespace framework::graphics;
    ClipSrc run;
    run.name = "hero/run";
    run.flags = CLIP_LOOP;
    run.texture_guid = 0xA1;
    run.frames = {
        ClipFrameSrc{0, 0, 16, 24, 8, 24, 3, "step",
                     {ClipBoxSrc{BoxKind::Push, 0, {-4, -20, 8, 20}}, ClipBoxSrc{BoxKind::Hurt, 1, {-6, -24, 12, 8}},
                      ClipBoxSrc{BoxKind::Hurt, 0, {-6, -16, 12, 16}}}},
        ClipFrameSrc{16, 0, 16, 24, 8, 24, 4, "", {}},
        ClipFrameSrc{32, 0, 16, 24, 8, 24, 3, "step", {ClipBoxSrc{BoxKind::Hit, 0, {2, -18, 10, 6}}}},
    };
    ClipSrc jab;
    jab.name = "hero/jab";
    jab.flags = CLIP_PINGPONG;
    jab.texture_guid = 0xB2;
    jab.frames = {
        ClipFrameSrc{0, 24, 20, 24, 10, 24, 2, "swing", {}},
        ClipFrameSrc{20, 24, 20, 24, -3, 30, 5, "", {ClipBoxSrc{BoxKind::Hit, 3, {4, -16, 12, 4}}}},
    };
    return {run, jab};
}

inline FakeSource source() {
    FakeSource s;
    s.images = {{"brawler.png", {0xB1, 128, 32}}, {"trim.png", {0x7A, 32, 20}}};
    return s;
}

inline const std::string BRAWLER_JSON = R"({ "frames": [
  { "filename": "brawler 0.aseprite", "frame": { "x": 0, "y": 0, "w": 32, "h": 32 }, "rotated": false,
    "trimmed": false, "spriteSourceSize": { "x": 0, "y": 0, "w": 32, "h": 32 },
    "sourceSize": { "w": 32, "h": 32 }, "duration": 100 },
  { "filename": "brawler 1.aseprite", "frame": { "x": 32, "y": 0, "w": 32, "h": 32 }, "rotated": false,
    "trimmed": false, "spriteSourceSize": { "x": 0, "y": 0, "w": 32, "h": 32 },
    "sourceSize": { "w": 32, "h": 32 }, "duration": 100 },
  { "filename": "brawler 2.aseprite", "frame": { "x": 64, "y": 0, "w": 32, "h": 32 }, "rotated": false,
    "trimmed": false, "spriteSourceSize": { "x": 0, "y": 0, "w": 32, "h": 32 },
    "sourceSize": { "w": 32, "h": 32 }, "duration": 100 },
  { "filename": "brawler 3.aseprite", "frame": { "x": 96, "y": 0, "w": 32, "h": 32 }, "rotated": false,
    "trimmed": false, "spriteSourceSize": { "x": 0, "y": 0, "w": 32, "h": 32 },
    "sourceSize": { "w": 32, "h": 32 }, "duration": 100 }
 ],
 "meta": { "app": "https://www.aseprite.org/", "version": "1.3.7-x64",
  "image": "C:\\art\\brawler.png", "format": "RGBA8888", "size": { "w": 128, "h": 32 }, "scale": "1",
  "frameTags": [
   { "name": "idle", "from": 0, "to": 1, "direction": "forward", "color": "#000000ff" },
   { "name": "punch", "from": 1, "to": 3, "direction": "forward", "repeat": "1", "color": "#000000ff",
     "data": "1:swing" },
   { "name": "back", "from": 0, "to": 2, "direction": "reverse", "color": "#000000ff" },
   { "name": "bob", "from": 0, "to": 3, "direction": "pingpong", "color": "#000000ff" },
   { "name": "bobr", "from": 0, "to": 3, "direction": "pingpong_reverse", "repeat": "1", "color": "#000000ff",
     "data": " 0 : step " }
  ],
  "layers": [ { "name": "body", "opacity": 255, "blendMode": "normal" } ],
  "slices": [
   { "name": "hurt0", "color": "#0000ffff", "keys": [
     { "frame": 0, "bounds": { "x": 8, "y": 4, "w": 16, "h": 28 } },
     { "frame": 2, "bounds": { "x": 0, "y": 0, "w": 0, "h": 0 } },
     { "frame": 3, "bounds": { "x": 10, "y": 4, "w": 16, "h": 28 } } ] },
   { "name": "hit0", "color": "#ff0000ff", "keys": [
     { "frame": 2, "bounds": { "x": 16, "y": 8, "w": 16, "h": 8 } },
     { "frame": 3, "bounds": { "x": 0, "y": 0, "w": 0, "h": 0 } } ] }
  ]
 }
})";

inline const std::string BRAWLER_SHEET = R"(# the brawler of BRAWLER_JSON, cell by cell
image|brawler.png
grid|32|32
clip|idle|row=0|from=0|to=1|ticks=6|loop
box|idle|0|hurt0|-8|-28|16|28
box|idle|1|hurt0|-8|-28|16|28
clip|punch|row=0|from=1|to=3|ticks=6
box|punch|1|hit0|0|-24|16|8
box|punch|2|hurt0|-6|-28|16|28
event|punch|1|swing
clip|back|row=0|from=2|to=0|ticks=6|loop
box|back|0|hit0|0|-24|16|8
box|back|1|hurt0|-8|-28|16|28
box|back|2|hurt0|-8|-28|16|28
clip|bob|row=0|from=0|to=3|ticks=6|loop|pingpong
box|bob|0|hurt0|-8|-28|16|28
box|bob|1|hurt0|-8|-28|16|28
box|bob|2|hit0|0|-24|16|8
box|bob|3|hurt0|-6|-28|16|28
clip|bobr|row=0|from=3|to=0|ticks=6
box|bobr|0|hurt0|-6|-28|16|28
box|bobr|1|hit0|0|-24|16|8
box|bobr|2|hurt0|-8|-28|16|28
box|bobr|3|hurt0|-8|-28|16|28
event|bobr|3|step
)";

inline const std::string TRIM_JSON = R"({ "frames": {
  "trim 0.ase": { "frame": { "x": 0, "y": 0, "w": 10, "h": 12 }, "rotated": false, "trimmed": true,
    "spriteSourceSize": { "x": 3, "y": 8, "w": 10, "h": 12 }, "sourceSize": { "w": 16, "h": 20 },
    "duration": 100 },
  "trim 1.ase": { "frame": { "x": 16, "y": 0, "w": 16, "h": 20 }, "rotated": false, "trimmed": false,
    "spriteSourceSize": { "x": 0, "y": 0, "w": 16, "h": 20 }, "sourceSize": { "w": 16, "h": 20 },
    "duration": 250 }
 },
 "meta": { "image": "trim.png", "size": { "w": 32, "h": 20 },
  "frameTags": [ { "name": "t", "from": 0, "to": 1, "direction": "forward" },
                 { "name": "late", "from": 1, "to": 1, "direction": "forward" } ],
  "slices": [
   { "name": "pivot", "keys": [ { "frame": 0, "bounds": { "x": 6, "y": 10, "w": 2, "h": 2 },
                                  "pivot": { "x": 1, "y": 1 } } ] },
   { "name": "hit0", "keys": [ { "frame": 0, "bounds": { "x": 7, "y": 11, "w": 9, "h": 2 } } ] }
  ]
 }
})";

inline const std::string QUEEN_SHEET = R"(image|queen.png
grid|74|75
clip|Walk|row=0|from=0|to=2|ticks=6|loop
clip|Death|row=0|from=3|to=8|ticks=6|loop
clip|Hit|row=0|from=3|to=3|ticks=6|loop
clip|Jab|row=0|from=9|to=9|ticks=6|loop
clip|Hook|row=0|from=10|to=10|ticks=6|loop
clip|Uppercut|row=0|from=11|to=11|ticks=6|loop
clip|Jump|row=0|from=12|to=12|ticks=6|loop
clip|Jump_Attack|row=0|from=13|to=13|ticks=6|loop
clip|Throw|row=0|from=14|to=14|ticks=6|loop
clip|Walk+Knuckles|row=0|from=15|to=17|ticks=6|loop
clip|Hit+Knuckles|row=0|from=18|to=18|ticks=6|loop
clip|Jab+Knuckles|row=0|from=19|to=19|ticks=6|loop
clip|Hook+Knuckles|row=0|from=20|to=20|ticks=6|loop
clip|Uppercut+Knuckles|row=0|from=21|to=21|ticks=6|loop
clip|Jump+Knuckles|row=0|from=22|to=22|ticks=6|loop
clip|Jump_Attack+Knuckles|row=0|from=23|to=23|ticks=6|loop
clip|KnuckleBuster_Charge|row=0|from=24|to=24|ticks=6|loop
clip|KnuckleBuster_Punch|row=0|from=25|to=25|ticks=6|loop
clip|Win|row=0|from=26|to=27|ticks=6|loop
clip|Win_Jump|row=0|from=28|to=28|ticks=6|loop
)";

} // namespace clip_fixture
