#pragma once
#include <vector>

#include "music_bake.hpp"

namespace music_fixture {
inline framework::music::MusicSrc one() {
    framework::music::MusicSrc row;
    row.name = "menu"; row.guid = 0x0102030405060708ull; row.frames = 96000;
    row.loop_start = 12000; row.loop_end = 84000; row.fade_ms = 750; return row;
}
inline std::vector<framework::music::MusicSrc> rows() {
    auto menu = one(), intro = one(), world = one();
    intro.name = "intro"; intro.frames = 48000; intro.loop_start = 0; intro.loop_end.reset(); intro.fade_ms = 1000;
    world.name = "world"; world.guid = 0; world.channels = 1; world.fade_ms = 0;
    return {world, menu, intro};
}
}
