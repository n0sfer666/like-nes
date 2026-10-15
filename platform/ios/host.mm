#import <UIKit/UIKit.h>

#include <sys/utsname.h>

#include "mobile_host.hpp"

namespace mobile {

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
