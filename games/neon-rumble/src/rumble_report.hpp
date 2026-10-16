#pragma once
#include <string>

#include "rumble_credits.hpp"
#include "rumble_level.hpp"

namespace rumble {

bool report_library_bundle(const std::string& path);
void report_level(const Level& level);
void report_credits(const Level& level, const Credits& credits);

} // namespace rumble
