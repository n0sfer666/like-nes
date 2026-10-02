#pragma once
#include <span>
#include <string>

#include "grid.hpp"

namespace framework::tilemap {

bool parse_flag_words(std::span<const std::string> words, TileFlags& out, std::string& error);

} // namespace framework::tilemap
