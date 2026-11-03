#pragma once
#include <string>

namespace ui_input_fixture {
inline const std::string PRESET =
    "preset|ui\n"
    "action|up|key:up|pad:dpup\n"
    "action|down|key:down|pad:dpdown\n"
    "action|left|key:left|pad:dpleft\n"
    "action|right|key:right|pad:dpright\n"
    "action|accept|key:enter|pad:south\n"
    "action|back|key:esc|pad:east\n"
    "action|erase|key:backspace|pad:west\n"
    "axis|x|padaxis:lx|-\n"
    "axis|y|padaxis:ly|-\n"
    "shape|x|0.25|1.0|1|-\n"
    "shape|y|0.25|1.0|1|-\n";
}
