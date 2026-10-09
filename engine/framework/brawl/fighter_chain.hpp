#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "fighter_bake.hpp"

namespace framework::brawl {

bool parse_chain(const std::string& value, int line, FighterSpec& f, FighterBakeError& err);
bool chain_moves(const FighterSpec& f, std::vector<uint32_t>& out, FighterBakeError& err);
bool check_chain_cancels(const FighterSpec& f, std::span<const graphics::ClipSrc> clips, FighterBakeError& err);

} // namespace framework::brawl
