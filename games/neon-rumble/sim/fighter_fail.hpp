#pragma once
#include <string>

#include "fighter_bake.hpp"

namespace framework::brawl {

inline bool fighter_fail(FighterBakeError& err, int line, const std::string& message) {
    err.line = line;
    err.message = message;
    return false;
}

inline std::string move_label(const MoveSpec& m) { return "move '" + m.name + "' hit" + std::to_string(m.strike.box); }

} // namespace framework::brawl
