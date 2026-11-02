#include <jni.h>
#include <android/input.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "android_pad_device.hpp"
#include "android_haptic.hpp"
#include "platform_haptic.hpp"
#include "platform_args.hpp"

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc != 2) return 1;
    std::string classpath = "-Djava.class.path=" + std::string(argv[1]);
    std::string checked = "-Xcheck:jni";
    JavaVMOption options[2]{};
    options[0].optionString = classpath.data(); options[1].optionString = checked.data();
    JavaVMInitArgs init{};
    init.version = JNI_VERSION_1_6; init.nOptions = 2; init.options = options;
    JavaVM* vm = nullptr;
    void* raw = nullptr;
    if (JNI_CreateJavaVM(&vm, &raw, &init) != JNI_OK) return 1;
    auto* env = static_cast<JNIEnv*>(raw);
    int fails = 0;
    const auto check = [&](bool good, const char* why) {
        if (!good) { ++fails; std::printf("android-pad-jvm: FAIL %s\n", why); }
    };
    const jclass device_class = env->FindClass("android/view/InputDevice");
    if (env->ExceptionCheck() || !device_class) return 1;
    const jfieldID scenario = env->GetStaticFieldID(device_class, "scenario", "I");
    if (env->ExceptionCheck() || !scenario) return 1;
    const auto mode = [&](int value) { env->SetStaticIntField(device_class, scenario, value); };
    std::array<jint, 128> ids{};
    size_t count = 0;
    check(android_pad_ids(vm, ids, count) && count == 3 && ids[1] == 44, "real JNI int array");
    AndroidPadDevice info;
    check(android_pad_device(vm, 12, info) && info.gamepad && info.info.vid == 0x045e && info.info.pid == 0x0b12 &&
          info.rx == AMOTION_EVENT_AXIS_Z && info.ry == AMOTION_EVENT_AXIS_RZ, "actual InputDevice metadata and ranges");
    check(android_pad_device(vm, 44, info) && info.gamepad && info.info.vid == 0 && info.rx == AMOTION_EVENT_AXIS_RX &&
          std::strcmp(info.info.name, "DualSense \u2603") == 0, "UTF-8 name and alternate right stick");
    check(android_pad_device(vm, 90, info) && !info.gamepad, "keyboard excluded by actual getSources");
    mode(1); check(!android_pad_ids(vm, ids, count) && !env->ExceptionCheck(), "inventory exception cleared");
    mode(2); check(!android_pad_device(vm, 12, info) && !env->ExceptionCheck(), "metadata exception cleared");
    mode(3); check(android_pad_device(vm, 12, info) && !info.gamepad, "removed device is normal");
    mode(5); check(!android_pad_ids(vm, ids, count) && count == 0 && !env->ExceptionCheck(), "oversized inventory bounded");
    mode(0);
    bool worker_ok = true;
    std::thread worker([&] {
        for (int i = 0; i < 50; ++i) {
            AndroidPadDevice from_thread;
            worker_ok = android_pad_device(vm, 12, from_thread) && from_thread.info.pid == 0x0b12 && worker_ok;
        }
    });
    worker.join();
    check(worker_ok, "real native thread attaches/detaches and finds platform classes");
    check(android_pad_rumble(vm, 12, .5f, 30), "actual getDevice/getVibrator/createOneShot chain");
    const jclass vibrator = env->FindClass("android/os/Vibrator");
    if (env->ExceptionCheck() || !vibrator) return 1;
    const jfieldID amplitude = env->GetStaticFieldID(vibrator, "amplitude", "I");
    if (env->ExceptionCheck() || !amplitude) return 1;
    const jfieldID duration = env->GetStaticFieldID(vibrator, "duration", "J");
    if (env->ExceptionCheck() || !duration) return 1;
    const int value = env->GetStaticIntField(vibrator, amplitude);
    if (env->ExceptionCheck()) return 1;
    check(value == 128 && env->GetStaticLongField(vibrator, duration) == 30, "real JNI long/int argument ABI");
    mode(4); check(!android_pad_rumble(vm, 12, 1, 30) && !env->ExceptionCheck(), "absent vibrator no-op");
    mode(0); check(android_pad_rumble(vm, 12, 0, 0), "actual vibrator cancel");
    const jclass context_class = env->FindClass("android/content/Context");
    if (env->ExceptionCheck() || !context_class) return 1;
    const jmethodID constructor = env->GetMethodID(context_class, "<init>", "()V");
    if (env->ExceptionCheck() || !constructor) return 1;
    const jobject local = env->NewObject(context_class, constructor);
    if (env->ExceptionCheck() || !local) return 1;
    const jobject context = env->NewGlobalRef(local);
    if (env->ExceptionCheck() || !context) return 1;
    env->DeleteLocalRef(local);
    android_haptic_begin(vm, context);
    check(haptic::phone::init(), "phone service and capability through actual JNI");
    haptic::phone::pulse(.25f, 60000);
    check(env->GetStaticIntField(vibrator, amplitude) == 64, "phone routes real one-shot amplitude");
    mode(8); haptic::phone::pulse(1, 20);
    check(!env->ExceptionCheck(), "phone vibration exception cleared");
    mode(0); haptic::phone::cancel();
    const jfieldID cancels = env->GetStaticFieldID(vibrator, "cancels", "I");
    if (env->ExceptionCheck() || !cancels) return 1;
    check(env->GetStaticIntField(vibrator, cancels) == 2, "cancel previous own effect even after failed pulse");
    mode(4); check(!haptic::phone::init(), "no phone motor");
    mode(6); check(!haptic::phone::init() && !env->ExceptionCheck(), "phone service exception cleared");
    mode(7); check(!haptic::phone::init() && !env->ExceptionCheck(), "phone capability exception cleared");
    mode(0); android_haptic_end();
    check(!haptic::phone::init(), "borrowed activity/VM cleared on teardown");
    env->DeleteGlobalRef(context); env->DeleteLocalRef(context_class);
    env->DeleteLocalRef(vibrator); env->DeleteLocalRef(device_class);
    check(vm->DestroyJavaVM() == JNI_OK, "VM shuts down without leaked attached native threads");
    std::puts(fails ? "android-pad-jvm: FAIL" : "android-pad-jvm: PASS real JVM checked JNI; modeled Android API, no hardware claim");
    return fails ? 1 : 0;
}
