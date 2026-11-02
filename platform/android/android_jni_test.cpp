#include "android_jni.hpp"

#include <cstdio>

namespace {

int calls, fail_at, forbidden, clears, frames;
bool pending;
_jobject object;
_jclass type;
_jstring text;

bool call() {
    if (pending) ++forbidden;
    if (++calls == fail_at) { pending = true; return false; }
    return true;
}

jint JNICALL push(JNIEnv*, jint) { if (!call()) return JNI_ERR; ++frames; return JNI_OK; }
jobject JNICALL pop(JNIEnv*, jobject) { --frames; return nullptr; }
jboolean JNICALL exception(JNIEnv*) { return pending ? JNI_TRUE : JNI_FALSE; }
void JNICALL clear(JNIEnv*) { pending = false; ++clears; }
jclass JNICALL klass(JNIEnv*, jobject) { return call() ? &type : nullptr; }
jclass JNICALL find(JNIEnv*, const char*) { return call() ? &type : nullptr; }
jmethodID JNICALL method(JNIEnv*, jclass, const char*, const char*) {
    return call() ? reinterpret_cast<jmethodID>(&object) : nullptr;
}
jfieldID JNICALL field(JNIEnv*, jclass, const char*, const char*) {
    return call() ? reinterpret_cast<jfieldID>(&object) : nullptr;
}
jobject JNICALL get_object(JNIEnv*, jobject, jmethodID, va_list) { return call() ? &object : nullptr; }
jint JNICALL get_int(JNIEnv*, jobject, jmethodID, va_list) { return call() ? 1 : 0; }
jint JNICALL get_static(JNIEnv*, jclass, jmethodID, va_list) { return call() ? 1 : 0; }
jboolean JNICALL get_bool(JNIEnv*, jobject, jmethodID, va_list) { return call() ? JNI_TRUE : JNI_FALSE; }
jint JNICALL get_field(JNIEnv*, jobject, jfieldID) { return call() ? 1 : 0; }
void JNICALL invoke(JNIEnv*, jobject, jmethodID, va_list) { (void)call(); }
jstring JNICALL new_text(JNIEnv*, const char*) { return call() ? &text : nullptr; }
jsize JNICALL length(JNIEnv*, jstring) { return call() ? 4 : 0; }
const char* JNICALL chars(JNIEnv*, jstring, jboolean*) { return call() ? "test" : nullptr; }
void JNICALL release(JNIEnv*, jstring, const char*) { (void)call(); }

JNINativeInterface_ functions() {
    JNINativeInterface_ f{};
    f.PushLocalFrame = push; f.PopLocalFrame = pop;
    f.ExceptionCheck = exception; f.ExceptionClear = clear;
    f.GetObjectClass = klass; f.FindClass = find;
    f.GetMethodID = method; f.GetStaticMethodID = method; f.GetFieldID = field;
    f.CallObjectMethodV = get_object; f.CallIntMethodV = get_int;
    f.CallStaticIntMethodV = get_static; f.GetIntField = get_field; f.CallVoidMethodV = invoke;
    f.CallBooleanMethodV = get_bool;
    f.NewStringUTF = new_text; f.GetStringUTFLength = length;
    f.GetStringUTFChars = chars; f.ReleaseStringUTFChars = release;
    return f;
}

bool exercise(JNIEnv& env, int failure) {
    calls = forbidden = clears = frames = 0;
    fail_at = failure;
    pending = false;
    bool ok;
    {
        AndroidJni j(&env);
        (void)j.object(&object, "getIntent", "()Landroid/content/Intent;");
        const jstring key = j.text("probe");
        const jstring nonce = static_cast<jstring>(j.object(&object, "getStringExtra",
            "(Ljava/lang/String;)Ljava/lang/String;", key));
        (void)j.utf8(nonce, 64);
        (void)j.integer(&object, "getRotation");
        (void)j.static_integer("android/view/WindowInsets$Type", "systemBars");
        (void)j.field(&object, "left");
        j.invoke(&object, "hide", "(I)V", jint{7});
        (void)j.boolean(&object, "getBooleanExtra", "(Ljava/lang/String;Z)Z", key, jboolean{JNI_FALSE});
        ok = j.ok();
    }
    return ok == (failure == 0) && forbidden == 0 && !pending && frames == 0 &&
        clears == (failure == 0 ? 0 : 1);
}

} // namespace

int main() {
    const JNINativeInterface_ f = functions();
    JNIEnv env{&f};
    if (!exercise(env, 0)) { std::puts("android-jni: FAIL normal calls"); return 1; }
    const int total = calls;
    for (int i = 1; i <= total; ++i) {
        if (!exercise(env, i)) { std::printf("android-jni: FAIL exception at call %d\n", i); return 1; }
    }
    std::printf("android-jni: PASS normal + %d injected exception paths\n", total);
    return 0;
}
