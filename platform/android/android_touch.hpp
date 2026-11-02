#pragma once
#include <android/input.h>

#include "mobile_app.hpp"

int32_t android_touch(mobile::Game& game, AInputEvent* event, float scale, const std::string& probe);
