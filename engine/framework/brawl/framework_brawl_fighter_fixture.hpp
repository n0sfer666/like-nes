#pragma once
#include <string>
#include <vector>

#include "clip_bake.hpp"
#include "fighter.hpp"

namespace framework::brawl::test {

inline const char* FIGHTER_TEXT =
    "\n"
    "# comments and blank lines are part of the grammar, so the fixture carries them\n"
    "sheet   | banderas\n"
    "speed_x | 2\n"
    "speed_z | 1\n"
    "run_x   | 3\n"
    "gravity | 0.5\n"
    "jump_vy | 6\n"
    "depth   | 4      # the tail after # is dropped\n"
    "hp      | 120\n"
    "down    | 30\n"
    "getup   | 24\n"
    "\n"
    "move | jab | hit0\n"
    "type      | light\n"
    "damage    | 8\n"
    "depth     | 5\n"
    "hitstop   | 4\n"
    "hitstun   | 12\n"
    "knock_x   | 1.5\n"
    "knock_y   | 0\n"
    "hits_down | no\n"
    "\n"
    "move | kick | hit1\n"
    "type      | launch\n"
    "damage    | 20\n"
    "depth     | 6.25\n"
    "hitstop   | 7\n"
    "hitstun   | 30\n"
    "knock_x   | 3\n"
    "knock_y   | 4.75\n"
    "hits_down | yes\n";

inline graphics::ClipSrc fixture_clip(const std::string& name, uint8_t hit) {
    graphics::ClipSrc c;
    c.name = name;
    c.frames.resize(2);
    c.frames[0].boxes.push_back({graphics::BoxKind::Hurt, 0, {0, 0, 8, 8}});
    c.frames[1].boxes.push_back({graphics::BoxKind::Hit, hit, {8, 0, 4, 4}});
    return c;
}

inline bool same_strike(const Strike& a, const Strike& b) {
    return a.box == b.box && a.type == b.type && a.hits_down == b.hits_down && a.damage == b.damage &&
           a.depth == b.depth && a.hitstop == b.hitstop && a.hitstun == b.hitstun && a.knock_x == b.knock_x &&
           a.knock_y == b.knock_y;
}

inline std::vector<graphics::ClipSrc> fixture_clips() {
    return {fixture_clip("banderas/idle", 0), fixture_clip("banderas/jab", 0), fixture_clip("banderas/kick", 1)};
}

} // namespace framework::brawl::test
