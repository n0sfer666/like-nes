#include "android_vm.hpp"

#include <cstdio>
#include <initializer_list>

namespace {
int get_calls, attaches, detaches;
jint status, attach_status;
JNIEnv env;

jint JNICALL get(JavaVM*, void** out, jint version) {
    ++get_calls;
    if (version != JNI_VERSION_1_6) return JNI_EVERSION;
    if (status == JNI_OK) *out = &env;
    return status;
}
template<class Env>
jint JNICALL attach(JavaVM*, Env** out, void*) {
    ++attaches;
    if (attach_status == JNI_OK) *out = &env;
    return attach_status;
}
jint JNICALL detach(JavaVM*) { ++detaches; return JNI_OK; }
}

int main() {
    JNIInvokeInterface_ f{};
    f.GetEnv = get; f.AttachCurrentThread = attach; f.DetachCurrentThread = detach;
    JavaVM vm{&f};
    for (jint initial : {JNI_OK, JNI_EDETACHED, JNI_EVERSION}) {
        for (jint result : {JNI_OK, JNI_ERR}) {
            get_calls = attaches = detaches = 0;
            status = initial; attach_status = result;
            const bool owns = initial == JNI_EDETACHED && result == JNI_OK;
            const bool ready = initial == JNI_OK || owns;
            {
                AndroidVm scope(&vm);
                if ((scope.env() == &env) != ready || detaches != 0) return 1;
            }
            if (get_calls != 1 || attaches != (initial == JNI_EDETACHED ? 1 : 0) || detaches != (owns ? 1 : 0))
                return 1;
        }
    }
    AndroidVm absent(nullptr);
    if (absent.env() != nullptr) return 1;
    std::puts("android-vm: PASS existing attachment, owned attachment, failures, absent VM");
    return 0;
}
