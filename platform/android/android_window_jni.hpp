#pragma once
#include <android/native_activity.h>

#include "android_window.hpp"

// Только из callback UI-looper или ANativeActivity: activity->env принадлежит UI-потоку.
bool android_window_sample(ANativeActivity* activity, AndroidWindowSnapshot& out);
bool android_window_immersive(ANativeActivity* activity);
