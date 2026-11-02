#pragma once
#include "scene_object.hpp"

namespace framework::scene {

bool read_path(const SceneObject& object, Path& out);
bool read_autoscroll(const SceneObject& object, Autoscroll& out);

} // namespace framework::scene
