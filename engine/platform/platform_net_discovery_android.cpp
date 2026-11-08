#include "platform_net_discovery_backend.hpp"

#include <android/native_activity.h>

#include "android_host.hpp"
#include "android_jni.hpp"
#include "android_vm.hpp"

namespace platform::net {
namespace {

class AndroidDiscovery final : public DiscoveryBackend {
public:
    ~AndroidDiscovery() override;
    bool open(const DiscoveryConfig& config) override;
    void poll(uint64_t now) override;

private:
    void event(const std::string& message);
    JavaVM* vm_ = nullptr;
    jobject bridge_ = nullptr;
};

bool AndroidDiscovery::open(const DiscoveryConfig& config) {
    auto* activity = mobile::native_activity();
    if (!activity || config.local_only) return false;
    vm_ = activity->vm;
    AndroidVm thread(vm_);
    auto* env = thread.env();
    if (!env) return false;
    AndroidJni j(env);
    jobject loader = j.object(activity->clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (!loader) return false;
    auto type = static_cast<jclass>(j.object(loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;",
                                             j.text("org.likenes.DiscoveryBridge")));
    if (!type || !j.ok()) return false;
    jmethodID ctor = env->GetMethodID(type, "<init>", "(Landroid/content/Context;)V");
    if (!ctor || !j.ok()) return false;
    jobject bridge = env->NewObject(type, ctor, activity->clazz);
    if (!bridge || !j.ok()) return false;
    bridge_ = env->NewGlobalRef(bridge);
    if (!bridge_ || !j.ok()) return false;
    return j.boolean(bridge_, "start", "(Ljava/lang/String;I)Z", j.text(config.name.c_str()),
                     static_cast<jint>(config.port));
}

AndroidDiscovery::~AndroidDiscovery() {
    if (!bridge_) return;
    AndroidVm thread(vm_);
    if (auto* env = thread.env()) {
        AndroidJni j(env);
        j.invoke(bridge_, "close", "()V");
        env->DeleteGlobalRef(bridge_);
    }
}

void AndroidDiscovery::poll(uint64_t) {
    if (!good_ || !bridge_) return;
    AndroidVm thread(vm_);
    if (!thread.env()) {
        good_ = false;
        return;
    }
    AndroidJni j(thread.env());
    for (size_t i = 0; i < DISCOVERY_ROOMS; ++i) {
        auto next = static_cast<jstring>(j.object(bridge_, "pop", "()Ljava/lang/String;"));
        if (!next) break;
        event(j.utf8(next, 200));
        if (!good_) break;
    }
    good_ = good_ && j.ok();
}

void AndroidDiscovery::event(const std::string& message) {
    if (message == "!") good_ = false;
    if (message.empty()) return;
    if (message.front() == '-') {
        remove(std::string_view(message).substr(1), 0);
        return;
    }
    if (message.front() != '+') return;
    const auto split = message.find('\t');
    Endpoint endpoint;
    if (split != std::string::npos && parse_endpoint(message.substr(split + 1), &endpoint)) {
        add(std::string_view(message).substr(1, split - 1), endpoint, 0);
    }
}

} // namespace

std::unique_ptr<DiscoveryBackend> make_discovery_backend() { return std::make_unique<AndroidDiscovery>(); }

} // namespace platform::net
