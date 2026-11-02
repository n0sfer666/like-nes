#import <UIKit/UIKit.h>

#include <sys/utsname.h>
#include <pthread/qos.h>

#include "mobile_host.hpp"

namespace mobile {

void interactive_thread() {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}


std::string asset_path(const char* name) {
    return std::string(NSBundle.mainBundle.bundlePath.UTF8String) + "/assets/" + name;
}

bool has_arg(const char* flag) {
    return [NSProcessInfo.processInfo.arguments containsObject:@(flag)];
}

std::string device_info() {
    utsname host = {};
    uname(&host);
    return std::string("iOS ") + UIDevice.currentDevice.systemVersion.UTF8String + ", " + host.machine;
}

} // namespace mobile
