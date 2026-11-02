#pragma once
#include <string>

#include "bundle_view.hpp"
#include "viewport_settings.hpp"

namespace framework::scene {

// Отсутствующая секция — стандартные размеры; присутствующая, но битая — отказ.
bool load_viewport_settings(asset::BundleView bundle, graphics::ViewportSettings& out, std::string& error);

} // namespace framework::scene
