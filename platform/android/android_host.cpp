#include "android_host.hpp"

#include <android/api-level.h>
#include <sys/system_properties.h>

#include <cstdio>
#include <utility>

#include "mobile_host.hpp"

namespace mobile {
namespace {

std::string& asset_dir() {
    static std::string dir;
    return dir;
}

std::string property(const char* name) {
    char value[PROP_VALUE_MAX] = {};
    return __system_property_get(name, value) > 0 ? value : "?";
}

} // namespace

void set_asset_dir(std::string dir) { asset_dir() = std::move(dir); }

std::string asset_path(const char* name) { return asset_dir() + "/" + name; }

// У NativeActivity нет argv: флаги запуска дойдут сюда через extras интента, когда понадобятся.
bool has_arg(const char*) { return false; }

std::string device_info() {
    char buf[256];
    std::snprintf(buf, sizeof buf, "Android %s (API %d), %s %s, soc %s",
                  property("ro.build.version.release").c_str(), android_get_device_api_level(),
                  property("ro.product.manufacturer").c_str(), property("ro.product.model").c_str(),
                  property("ro.soc.model").c_str());
    return buf;
}

} // namespace mobile
