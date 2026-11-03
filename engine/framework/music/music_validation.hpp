#pragma once
#include <string_view>

#include "music_format.hpp"

namespace framework::music::detail {
bool name_ok(std::string_view name);
bool row_ok(const MusicRow& row);
}
