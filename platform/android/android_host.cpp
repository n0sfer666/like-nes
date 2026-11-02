#include "android_host.hpp"

#include <android/api-level.h>
#include <sys/system_properties.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <utility>

#include "mobile_host.hpp"

namespace mobile {

void interactive_thread() {}

namespace {

std::string& asset_dir() {
    static std::string dir;
    return dir;
}

thread_local bool demo_mode = false;
thread_local std::string launch_args;

std::string property(const char* name) {
    char value[PROP_VALUE_MAX] = {};
    return __system_property_get(name, value) > 0 ? value : "?";
}

} // namespace

void set_asset_dir(std::string dir) { asset_dir() = std::move(dir); }
void set_demo(bool enabled) { demo_mode = enabled; }
void set_args(std::string flags) { launch_args = std::move(flags); }

std::string asset_path(const char* name) { return asset_dir() + "/" + name; }

// У NativeActivity нет argv: UI-поток читает extra, android_main передаёт их перед init игры.
bool has_arg(const char* flag) {
    if (std::strcmp(flag, "--demo") == 0 && demo_mode) return true;
    std::istringstream words(launch_args);
    std::string word;
    while (words >> word) if (word == flag) return true;
    return false;
}

std::string device_info() {
    char buf[256];
    std::snprintf(buf, sizeof buf, "Android %s (API %d), %s %s, soc %s",
                  property("ro.build.version.release").c_str(), android_get_device_api_level(),
                  property("ro.product.manufacturer").c_str(), property("ro.product.model").c_str(),
                  property("ro.soc.model").c_str());
    return buf;
}

} // namespace mobile
