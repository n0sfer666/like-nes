#pragma once
#include <algorithm>
#include <cmath>

#include "android_jni.hpp"

// Vibrator один и для телефона, и для InputDevice. Нулевой импульс — cancel;
// положительный требует amplitude1..255, поэтому округление не должно породить0.
inline bool android_vibrate(AndroidJni& j, jobject vibrator, float strength, int ms) {
    if (!vibrator || !j.boolean(vibrator, "hasVibrator", "()Z")) return false;
    if (!std::isfinite(strength) || strength <= 0 || ms <= 0) {
        j.invoke(vibrator, "cancel", "()V");
        return j.ok();
    }
    const int amplitude = std::clamp(static_cast<int>(std::lround(std::min(strength, 1.f) * 255)), 1, 255);
    const jobject effect = j.static_object("android/os/VibrationEffect", "createOneShot",
        "(JI)Landroid/os/VibrationEffect;", static_cast<jlong>(ms), jint{amplitude});
    if (!effect || !j.ok()) return false;
    j.invoke(vibrator, "vibrate", "(Landroid/os/VibrationEffect;)V", effect);
    return j.ok();
}
