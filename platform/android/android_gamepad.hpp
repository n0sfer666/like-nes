#pragma once
#include <android/input.h>
#include <jni.h>

// Один backend на игровой поток NativeActivity. Новый вход сбрасывает старые id/уровни;
// VM остаётся живой до выхода из android_main, JNIEnv берётся только у текущего потока.
void android_gamepad_begin(JavaVM* vm);
void android_gamepad_end();
void android_gamepad_refresh();
void android_gamepad_cancel();
int32_t android_gamepad_event(const AInputEvent* event);
