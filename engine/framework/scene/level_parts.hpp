#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "level_error.hpp"
#include "scene_level.hpp"

namespace framework::scene {

bool level_table(const Level& level, const std::string& table, const uint8_t*& data, std::size_t& size,
                 LoadError& error);


} // namespace framework::scene
