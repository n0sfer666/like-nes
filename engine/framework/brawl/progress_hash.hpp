#pragma once
#include <cstdint>

#include "level_progress.hpp"

namespace framework::brawl {

uint64_t progress_hash(const LevelProgress& progress);

} // namespace framework::brawl
