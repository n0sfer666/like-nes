#pragma once
#include <jni.h>

// activity->env принадлежит UI-потоку. Native gamepad опрашивается игровым потоком:
// берём его JNIEnv у VM и отсоединяем лишь соединение, созданное этим объектом.
class AndroidVm {
public:
    explicit AndroidVm(JavaVM* vm) : vm_(vm) {
        if (!vm_) return;
        void* raw = nullptr;
        const jint status = vm_->GetEnv(&raw, JNI_VERSION_1_6);
        if (status == JNI_OK) env_ = static_cast<JNIEnv*>(raw);
        else if (status == JNI_EDETACHED) {
            attached_ = attach(vm_->functions->AttachCurrentThread) == JNI_OK;
        }
    }
    ~AndroidVm() { if (attached_) vm_->DetachCurrentThread(); }
    AndroidVm(const AndroidVm&) = delete;
    AndroidVm& operator=(const AndroidVm&) = delete;
    JNIEnv* env() const { return env_; }

private:
    // NDK принимает JNIEnv**, host JDK — void**. Тип выводится из настоящей таблицы VM.
    template<class Env>
    jint attach(jint (JNICALL *call)(JavaVM*, Env**, void*)) {
        Env* value = nullptr;
        const jint status = call(vm_, &value, nullptr);
        if (status == JNI_OK) env_ = static_cast<JNIEnv*>(value);
        return status;
    }
    JavaVM* vm_;
    JNIEnv* env_ = nullptr;
    bool attached_ = false;
};
