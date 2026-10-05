#pragma once
#include <vector>

#include "font_bake.hpp"

namespace font_fixture {

inline framework::graphics::FontSrc small() {
    framework::graphics::FontSrc f;
    f.name = "tiny";
    f.line_height = 8;
    f.texture_guid = 0x1234;
    f.page_w = 32;
    f.page_h = 8;
    f.glyphs = {{0x416, 8, 0, 5, 6}, {' ', 0, 0, 0, 4}, {'A', 4, 0, 3, 4}, {'?', 0, 0, 3, 4}};
    return f;
}

} // namespace font_fixture
