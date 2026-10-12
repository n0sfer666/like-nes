#pragma once
#include "bundle_view.hpp"
#include "level_error.hpp"
#include "scene_level.hpp"

namespace framework::scene {

bool load_level(asset::BundleView bundle, const char* name, Level& out, LoadError& error);

} // namespace framework::scene
