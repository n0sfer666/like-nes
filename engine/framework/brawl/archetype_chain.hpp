#pragma once
#include <string>

#include "archetype.hpp"

namespace framework::brawl {

uint32_t cancel_tick(const graphics::ClipView& view);
bool make_chain(const FighterTable& fighter, const graphics::ClipTable& clips, Archetype& out, std::string& error);

} // namespace framework::brawl
