#include <cstdio>
#include <thread>

#include "android_host.hpp"
#include "mobile_host.hpp"

extern "C" int android_get_device_api_level() { return 35; }
extern "C" int __system_property_get(const char*, char* out) { out[0] = '\0'; return 0; }

int main() {
    int fails = 0;
    const auto check = [&](bool ok, const char* why) {
        if (!ok) { ++fails; std::printf("android-host: FAIL %s\n", why); }
    };
    mobile::set_demo(false);
    mobile::set_args(" --wave\t--another-mode\n");
    check(mobile::has_arg("--wave") && mobile::has_arg("--another-mode"), "generic flag tokens");
    check(!mobile::has_arg("--demo") && !mobile::has_arg("wave") && !mobile::has_arg("--wav"),
          "no implicit demo or substring matching");
    mobile::set_args("--wave-extra");
    check(!mobile::has_arg("--wave"), "a prefix is not a whole flag");
    mobile::set_demo(true);
    check(mobile::has_arg("--demo"), "legacy boolean demo extra is preserved");
    bool child_ok = false;
    std::thread child([&] {
        child_ok = !mobile::has_arg("--demo") && !mobile::has_arg("--wave-extra");
        mobile::set_args("--child");
    });
    child.join();
    check(child_ok && !mobile::has_arg("--child"), "launch options stay on their game thread");
    mobile::set_demo(false);
    mobile::set_args("");
    check(!mobile::has_arg("--demo") && !mobile::has_arg("--wave"), "fresh empty options reset flags");
    std::puts(fails ? "android-host: FAIL" : "android-host: PASS generic args, legacy demo, exact tokens, thread locality");
    return fails ? 1 : 0;
}
