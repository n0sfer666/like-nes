#pragma once
#include <jni.h>

#include <array>
#include <cstddef>

#include "source.hpp"

struct AndroidPadDevice {
    bool gamepad = false;
    input::PadInfo info;
    int rx = 0, ry = 0;
};

// Только JNI-паспорт/инвентарь; оси и кнопки приходят из AInputEvent без JNI.
bool android_pad_ids(JavaVM* vm, std::array<jint, 128>& ids, size_t& count);
bool android_pad_device(JavaVM* vm, int id, AndroidPadDevice& out);
bool android_pad_rumble(JavaVM* vm, int id, float strength, int ms);
