#pragma once
#include "bundle_view.hpp"
#include "level_error.hpp"
#include "scene_level.hpp"

namespace framework::scene {

// Подготовка не читает игровые классы и не грузит текстуры: игра вставляет свою проверку между этапами.
bool open_level(asset::BundleView bundle, const char* name, Level& out, LoadError& error);
// Дополнительные этапы работают с успешно открытым Level; при отказе его можно отбросить.
bool load_textures(Level& level, LoadError& error);
bool load_clips(Level& level, LoadError& error);

bool load_level(asset::BundleView bundle, const char* name, Level& out, LoadError& error);

} // namespace framework::scene
