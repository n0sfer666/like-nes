#pragma once
#include "level_objects.hpp"
#include "scene_read.hpp"

namespace framework::scene {

bool read_level_objects(const tilemap::ObjectMap& map, LevelObjects& out, std::string& error);

} // namespace framework::scene
