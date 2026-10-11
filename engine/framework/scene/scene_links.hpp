#pragma once
#include <string>

#include "object_read.hpp"
#include "scene_objects.hpp"

namespace framework::scene {

bool check_links(const tilemap::ObjectMap& map, const LevelObjects& level, std::string& error);

} // namespace framework::scene
