#pragma once
#include <android/input.h>

#include "input_types.hpp"

int android_pad_button(int keycode);
bool android_pad_source(int source);
fix32 android_pad_axis(float value, bool trigger = false);
