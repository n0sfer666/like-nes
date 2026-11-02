#pragma once
#include <jni.h>

#include <string>

// Локальные ссылки и исключения ограничены одним UI-запросом; после первой ошибки вызовов нет.
class AndroidJni {
public:
    explicit AndroidJni(JNIEnv* env) : env_(env), ok_(env->PushLocalFrame(64) == JNI_OK) {}
    ~AndroidJni() {
        if (env_->ExceptionCheck()) env_->ExceptionClear();
        if (ok_frame_) env_->PopLocalFrame(nullptr);
    }
    AndroidJni(const AndroidJni&) = delete;
    AndroidJni& operator=(const AndroidJni&) = delete;

    bool ok() { return check(); }
    jstring text(const char* value) {
        return check() ? env_->NewStringUTF(value) : nullptr;
    }
    std::string utf8(jstring value, size_t limit) {
        if (!check() || !value) return {};
        const jsize size = env_->GetStringUTFLength(value);
        if (!check() || static_cast<size_t>(size) > limit) return {};
        const char* chars = env_->GetStringUTFChars(value, nullptr);
        if (!chars) return {};
        const std::string result = chars;
        env_->ReleaseStringUTFChars(value, chars);
        return result;
    }
    template<class... A>
    jobject object(jobject obj, const char* name, const char* sig, A... args) {
        const jmethodID m = method(obj, name, sig);
        return m ? env_->CallObjectMethod(obj, m, args...) : nullptr;
    }
    int integer(jobject obj, const char* name) {
        const jmethodID m = method(obj, name, "()I");
        return m ? env_->CallIntMethod(obj, m) : 0;
    }
    int static_integer(const char* type, const char* name) {
        if (!check()) return 0;
        const jclass c = env_->FindClass(type);
        const jmethodID m = c ? env_->GetStaticMethodID(c, name, "()I") : nullptr;
        if (!m) ok_ = false;
        return check() ? env_->CallStaticIntMethod(c, m) : 0;
    }
    template<class... A>
    void invoke(jobject obj, const char* name, const char* sig, A... args) {
        const jmethodID m = method(obj, name, sig);
        if (m) env_->CallVoidMethod(obj, m, args...);
    }
    int field(jobject obj, const char* name) {
        if (!check() || !obj) { ok_ = false; return 0; }
        const jclass c = env_->GetObjectClass(obj);
        const jfieldID f = c ? env_->GetFieldID(c, name, "I") : nullptr;
        return f && check() ? env_->GetIntField(obj, f) : 0;
    }

private:
    bool check() { ok_ = ok_ && !env_->ExceptionCheck(); return ok_; }
    jmethodID method(jobject obj, const char* name, const char* sig) {
        if (!check() || !obj) { ok_ = false; return nullptr; }
        const jclass c = env_->GetObjectClass(obj);
        const jmethodID m = c ? env_->GetMethodID(c, name, sig) : nullptr;
        if (!m) ok_ = false;
        return check() ? m : nullptr;
    }
    JNIEnv* env_;
    bool ok_;
    bool ok_frame_ = ok_;
};
