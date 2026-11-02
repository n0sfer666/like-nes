#include "android_window_jni.hpp"

#include <algorithm>

#include "android_jni.hpp"

namespace {

jobject decor(AndroidJni& j, ANativeActivity* a, jobject& window) {
    window = j.object(a->clazz, "getWindow", "()Landroid/view/Window;");
    return j.object(window, "getDecorView", "()Landroid/view/View;");
}

void read_probe(AndroidJni& j, ANativeActivity* a, std::string& out) {
    const jobject intent = j.object(a->clazz, "getIntent", "()Landroid/content/Intent;");
    const jstring key = j.text("probe");
    const jstring nonce = static_cast<jstring>(j.object(intent, "getStringExtra",
        "(Ljava/lang/String;)Ljava/lang/String;", key));
    const std::string value = j.utf8(nonce, 64);
    // Интент — внешние данные, а нонс станет частью строки лога. Только один короткий токен.
    if (value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
        std::string::npos) out = value;
}

} // namespace

bool android_window_sample(ANativeActivity* a, AndroidWindowSnapshot& out) {
    AndroidJni j(a->env);
    AndroidWindowSnapshot next;
    jobject window = nullptr;
    const jobject view = decor(j, a, window);
    const jobject display = j.object(view, "getDisplay", "()Landroid/view/Display;");
    if (display) next.rotation = j.integer(display, "getRotation");
    const jobject insets = j.object(view, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
    if (insets) {
        next.insets = true;
        if (a->sdkVersion >= 30) {
            const int types = j.static_integer("android/view/WindowInsets$Type", "systemBars");
            const jobject bars = j.object(insets, "getInsets", "(I)Landroid/graphics/Insets;", types);
            next.left = j.field(bars, "left"); next.top = j.field(bars, "top");
            next.right = j.field(bars, "right"); next.bottom = j.field(bars, "bottom");
        } else {
            next.left = j.integer(insets, "getSystemWindowInsetLeft");
            next.top = j.integer(insets, "getSystemWindowInsetTop");
            next.right = j.integer(insets, "getSystemWindowInsetRight");
            next.bottom = j.integer(insets, "getSystemWindowInsetBottom");
        }
        const jobject cutout = j.object(insets, "getDisplayCutout", "()Landroid/view/DisplayCutout;");
        if (cutout) {
            next.left = std::max(next.left, j.integer(cutout, "getSafeInsetLeft"));
            next.top = std::max(next.top, j.integer(cutout, "getSafeInsetTop"));
            next.right = std::max(next.right, j.integer(cutout, "getSafeInsetRight"));
            next.bottom = std::max(next.bottom, j.integer(cutout, "getSafeInsetBottom"));
        }
    }
    read_probe(j, a, next.probe);
    if (!j.ok()) return false;
    out = next;
    return true;
}

bool android_window_immersive(ANativeActivity* a) {
    AndroidJni j(a->env);
    jobject window = nullptr;
    const jobject view = decor(j, a, window);
    if (a->sdkVersion >= 30) {
        j.invoke(window, "setDecorFitsSystemWindows", "(Z)V", jboolean{JNI_FALSE});
        const jobject controller = j.object(window, "getInsetsController", "()Landroid/view/WindowInsetsController;");
        if (controller) {
            j.invoke(controller, "setSystemBarsBehavior", "(I)V", jint{2}); // TRANSIENT_BARS_BY_SWIPE
            j.invoke(controller, "hide", "(I)V",
                     j.static_integer("android/view/WindowInsets$Type", "systemBars"));
        }
    } else {
        // LAYOUT_STABLE | LAYOUT_HIDE_NAVIGATION | LAYOUT_FULLSCREEN | HIDE_NAVIGATION |
        // FULLSCREEN | IMMERSIVE_STICKY — SDK29, без рефлексии на недоступный controller.
        j.invoke(view, "setSystemUiVisibility", "(I)V", jint{0x1706});
    }
    return j.ok();
}
