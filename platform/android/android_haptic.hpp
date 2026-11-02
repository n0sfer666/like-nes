#pragma once
#include <jni.h>

// clazz — уже глобальная ссылка NativeActivity, заимствуется только до end на том же потоке.
void android_haptic_begin(JavaVM* vm, jobject activity);
void android_haptic_end();
