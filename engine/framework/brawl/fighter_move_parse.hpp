#pragma once
#include <string>
#include <vector>

#include "fighter_bake.hpp"

namespace framework::brawl {

bool open_move(FighterSpec& f, const std::vector<std::string>& fields, int line, FighterBakeError& err);
bool set_move_text(MoveSpec& m, const std::string& key, const std::string& value, int line, FighterBakeError& err);
bool same_rows(const FighterSpec& f, FighterBakeError& err);

} // namespace framework::brawl
