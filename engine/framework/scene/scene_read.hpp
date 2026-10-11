#pragma once
#include <string>

#include "object_read.hpp"
#include "scene_objects.hpp"

namespace framework::scene {

bool read_level_objects(const tilemap::ObjectMap& map, LevelObjects& out, std::string& error);

} // namespace framework::scene
