#include "android_haptic.hpp"

#include <chrono>
#include <cstdio>

#include "android_jni.hpp"
#include "android_vibration.hpp"
#include "android_vm.hpp"
#include "platform_haptic.hpp"

namespace {
using Clock = std::chrono::steady_clock;
thread_local JavaVM* machine = nullptr;
thread_local jobject activity = nullptr;
thread_local bool available = false;
thread_local Clock::time_point until;

jobject vibrator(AndroidJni& j) {
    const jstring name = j.text("vibrator");
    return j.object(activity, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", name);
}
}

namespace haptic::phone {
bool init() {
    AndroidVm thread(machine);
    available = false;
    if (!thread.env() || !activity) return false;
    AndroidJni j(thread.env());
    const jobject motor = vibrator(j);
    available = motor && j.boolean(motor, "hasVibrator", "()Z") && j.ok();
    return available;
}
void pulse(float strength, int ms) {
    if (!available) return;
    AndroidVm thread(machine);
    if (!thread.env()) return;
    AndroidJni j(thread.env());
    if (!android_vibrate(j, vibrator(j), strength, ms)) {
        std::fprintf(stderr, "[haptic] phone JNI failed; disabled\n");
        available = false;
        return;
    }
    until = strength > 0 && ms > 0 ? Clock::now() + std::chrono::milliseconds(ms) : Clock::time_point{};
}
void cancel() {
    if (until > Clock::now()) {
        // Даже после ошибки нового vibrate пробуем остановить предыдущий свой эффект.
        AndroidVm thread(machine);
        if (thread.env()) {
            AndroidJni j(thread.env());
            (void)android_vibrate(j, vibrator(j), 0, 0);
        }
    }
    until = {};
}
void shutdown() { cancel(); available = false; }
}

void android_haptic_begin(JavaVM* vm, jobject context) {
    haptic::phone::shutdown(); machine = vm; activity = context;
}
void android_haptic_end() {
    haptic::phone::shutdown(); machine = nullptr; activity = nullptr;
}
