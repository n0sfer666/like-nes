#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "clip_bake.hpp"
#include "fighter.hpp"

namespace framework::brawl {

struct FighterBakeError {
    int line = 0;
    std::string message;
};

bool parse_fighter(const std::string& text, FighterSpec& out, FighterBakeError& err);
bool check_fighter_clips(const FighterSpec& spec, std::span<const graphics::ClipSrc> clips,
                         FighterBakeError& err);
bool bake_fighter(const std::string& name, const std::string& text,
                  std::span<const graphics::ClipSrc> clips, std::vector<uint8_t>& out,
                  FighterBakeError& err);

} // namespace framework::brawl
