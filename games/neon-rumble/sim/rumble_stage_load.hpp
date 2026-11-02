#pragma once
#include "rumble_stage.hpp"
#include "rumble_stage_error.hpp"

namespace rumble {

bool load_stage(asset::BundleView bundle, const char* name, Stage& out, StageError& error);
bool load_stage_fighters(Stage& level, StageError& error);

} // namespace rumble
