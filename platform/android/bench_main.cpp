#include <android_native_app_glue.h>
#include <sys/system_properties.h>

#include <cstdio>
#include <string>
#include <thread>

#include "apk_assets.hpp"
#include "resim_bench.hpp"
#include "stdout_logcat.hpp"

namespace {

std::string property(const char* name) {
    char value[PROP_VALUE_MAX] = {};
    return __system_property_get(name, value) > 0 ? value : "?";
}

} // namespace

void android_main(android_app* app) {
    game::stdout_to_logcat();
    const std::string dir = std::string(app->activity->internalDataPath) + "/bench";
    game::unpack_apk_asset(app->activity->assetManager, "crossplay.txt", dir);
    std::printf("[bench] Android %s (API %d), %s %s, soc %s\n", property("ro.build.version.release").c_str(),
                app->activity->sdkVersion, property("ro.product.manufacturer").c_str(),
                property("ro.product.model").c_str(), property("ro.soc.model").c_str());
    std::thread bench(framework::ai::bench::run_resim_bench, dir + "/crossplay.txt");
    while (!app->destroyRequested) {
        int events;
        android_poll_source* src;
        if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&src)) >= 0 && src)
            src->process(app, src);
    }
    bench.join();
}
