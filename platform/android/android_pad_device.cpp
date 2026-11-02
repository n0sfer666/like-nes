#include "android_pad_device.hpp"

#include <android/input.h>

#include <cstring>

#include "android_jni.hpp"
#include "android_vm.hpp"
#include "android_vibration.hpp"

namespace {

bool source(int value, int flag) { return (value & flag) == flag; }

uint16_t identifier(int value) { return value >= 0 && value <= 65535 ? static_cast<uint16_t>(value) : 0; }

} // namespace

bool android_pad_ids(JavaVM* vm, std::array<jint, 128>& ids, size_t& count) {
    AndroidVm thread(vm);
    if (!thread.env()) return false;
    AndroidJni j(thread.env());
    const auto array = static_cast<jintArray>(j.static_object("android/view/InputDevice", "getDeviceIds", "()[I"));
    return j.integers(array, ids.data(), ids.size(), count) && j.ok();
}

bool android_pad_device(JavaVM* vm, int id, AndroidPadDevice& out) {
    AndroidVm thread(vm);
    if (!thread.env()) return false;
    AndroidJni j(thread.env());
    const jobject device = j.static_object("android/view/InputDevice", "getDevice",
        "(I)Landroid/view/InputDevice;", jint{id});
    // Устройство могло исчезнуть между getDeviceIds и getDevice: это обычное отключение.
    if (!device) { out = {}; return j.ok(); }
    const int sources = j.integer(device, "getSources");
    AndroidPadDevice next;
    next.gamepad = source(sources, AINPUT_SOURCE_GAMEPAD) || source(sources, AINPUT_SOURCE_JOYSTICK);
    if (next.gamepad) {
        next.info.vid = identifier(j.integer(device, "getVendorId"));
        next.info.pid = identifier(j.integer(device, "getProductId"));
        const auto name = static_cast<jstring>(j.object(device, "getName", "()Ljava/lang/String;"));
        const std::string text = j.utf8(name, 512);
        std::strncpy(next.info.name, text.c_str(), sizeof next.info.name - 1);
        const jobject z = j.object(device, "getMotionRange", "(I)Landroid/view/InputDevice$MotionRange;",
                                   jint{AMOTION_EVENT_AXIS_Z});
        const jobject rz = j.object(device, "getMotionRange", "(I)Landroid/view/InputDevice$MotionRange;",
                                    jint{AMOTION_EVENT_AXIS_RZ});
        next.rx = z ? AMOTION_EVENT_AXIS_Z : AMOTION_EVENT_AXIS_RX;
        next.ry = rz ? AMOTION_EVENT_AXIS_RZ : AMOTION_EVENT_AXIS_RY;
    }
    if (!j.ok()) return false;
    out = next;
    return true;
}

bool android_pad_rumble(JavaVM* vm, int id, float strength, int ms) {
    AndroidVm thread(vm);
    if (!thread.env()) return false;
    AndroidJni j(thread.env());
    const jobject device = j.static_object("android/view/InputDevice", "getDevice",
        "(I)Landroid/view/InputDevice;", jint{id});
    if (!device) return false;
    // API29 поддерживает getVibrator; на API31+ этот совместимый метод остаётся доступен.
    const jobject vibrator = j.object(device, "getVibrator", "()Landroid/os/Vibrator;");
    return android_vibrate(j, vibrator, strength, ms);
}
