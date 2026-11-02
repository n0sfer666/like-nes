#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/native_window.h>

#include <webgpu/webgpu.h>

#include <cstdio>
#include <ctime>
#include <memory>

#include "android_host.hpp"
#include "android_gamepad.hpp"
#include "android_haptic.hpp"
#include "android_lifecycle.hpp"
#include "android_touch.hpp"
#include "android_window.hpp"
#include "apk_assets.hpp"
#include "gpu.hpp"
#include "haptic.hpp"
#include "mobile_app.hpp"
#include "mobile_probe.hpp"
#include "platform_env.hpp"
#include "platform_save.hpp"
#include "stdout_logcat.hpp"

namespace {

struct App {
    GpuContext gpu;
    std::unique_ptr<mobile::Game> game;
    WGPUSurface surface = nullptr;
    uint32_t fb_w = 0, fb_h = 0;
    float scale = 1;
    AndroidWindowSnapshot window;
    uint32_t frames = 0;
    double last = 0;
};

double now_s() {
    timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

bool make_surface(App& a, ANativeWindow* win) {
    const int32_t w = ANativeWindow_getWidth(win), h = ANativeWindow_getHeight(win);
    if (w <= 0 || h <= 0) return false;
    WGPUSurfaceDescriptorFromAndroidNativeWindow nw = {};
    nw.chain.sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow;
    nw.window = win;
    WGPUSurfaceDescriptor sd = {};
    sd.nextInChain = &nw.chain;
    a.surface = wgpuInstanceCreateSurface(a.gpu.instance, &sd);
    a.fb_w = static_cast<uint32_t>(w);
    a.fb_h = static_cast<uint32_t>(h);
    return a.surface != nullptr;
}

void release_surface(App& a) {
    if (a.surface) wgpuSurfaceRelease(a.surface);
    a.surface = nullptr;
}

mobile::SafeArea safe_area(const App& a) {
    return {static_cast<float>(a.window.left) / a.scale, static_cast<float>(a.window.top) / a.scale,
            static_cast<float>(a.window.right) / a.scale, static_cast<float>(a.window.bottom) / a.scale, a.scale};
}

void report_safe(const App& a) {
    if (!a.window.probe.empty()) std::fprintf(stderr, "%s\n",
        mobile::safe_line(a.fb_w, a.fb_h, safe_area(a), a.window.rotation, a.window.probe.c_str()).c_str());
}

void teardown(App& a) {
    if (a.game) {
        a.game->detach_surface();
        a.game->shutdown();
        a.game.reset();
    }
    release_surface(a);
    a.gpu.shutdown();
}

// Первое окно поднимает GPU и игру, каждое следующее (после APP_CMD_TERM_WINDOW) — только новую
// поверхность: игра пережила уход в фон. Отказ первого круга освобождает всё поднятое, иначе
// instance и поверхность копились бы с каждым INIT_WINDOW.
void attach(App& a, android_app* app) {
    a.scale = android_touch_scale(app->config);
    a.window = android_window_read(app->activity);
    if (!a.game) {
        a.gpu.instance = wgpuCreateInstance(nullptr);
        if (!make_surface(a, app->window) || !a.gpu.init(a.surface)) {
            std::printf("[game] gpu init failed\n");
            teardown(a);
            return;
        }
        a.game = mobile::make_game();
        if (!a.game->init(a.gpu)) {
            std::printf("[game] game init failed\n");
            teardown(a);
            return;
        }
    } else if (!make_surface(a, app->window)) {
        std::printf("[game] surface attach failed\n");
        release_surface(a);
        return;
    }
    if (!a.game->attach_surface(a.surface, a.fb_w, a.fb_h, safe_area(a))) {
        std::printf("[game] surface attach failed\n");
        release_surface(a);
    }
    a.last = 0;
    report_safe(a);
}

void detach(App& a) {
    if (a.game) { a.game->cancel(); a.game->detach_surface(); }
    release_surface(a);
}

// Смена плотности (перенос на другой экран, настройка размера шрифтов) меняет масштаб касаний и
// при прежнем размере окна, поэтому resize уходит игре и тогда.
void resize(App& a, android_app* app) {
    if (!a.game || !a.surface || !app->window) return;
    const int32_t w = ANativeWindow_getWidth(app->window), h = ANativeWindow_getHeight(app->window);
    const float scale = android_touch_scale(app->config);
    if (w <= 0 || h <= 0) return;
    const AndroidWindowSnapshot window = android_window_read(app->activity);
    if (static_cast<uint32_t>(w) == a.fb_w && static_cast<uint32_t>(h) == a.fb_h &&
        scale == a.scale && window == a.window) return;
    a.game->cancel();
    a.window = window;
    a.fb_w = static_cast<uint32_t>(w); a.fb_h = static_cast<uint32_t>(h); a.scale = scale;
    a.game->resize(a.fb_w, a.fb_h, safe_area(a));
    report_safe(a);
}

bool live(const App& a) { return a.game && a.surface && a.game->wants_frames(); }

int32_t on_input(android_app* app, AInputEvent* ev) {
    App& a = *static_cast<App*>(app->userData);
    if (android_gamepad_event(ev)) return 1;
    return a.game && a.surface ? android_touch(*a.game, ev, a.scale, a.window.probe) : 0;
}

void on_cmd(android_app* app, int32_t cmd) {
    App& a = *static_cast<App*>(app->userData);
    if (cmd == APP_CMD_LOST_FOCUS || cmd == APP_CMD_PAUSE) { android_gamepad_cancel(); haptic::suspend(); }
    if (cmd == APP_CMD_GAINED_FOCUS || cmd == APP_CMD_RESUME) haptic::resume();
    if (cmd == APP_CMD_RESUME) android_gamepad_refresh();
    if (cmd == APP_CMD_INIT_WINDOW && app->window) attach(a, app);
    else if (cmd == APP_CMD_TERM_WINDOW) detach(a);
    else if (cmd == APP_CMD_WINDOW_RESIZED || cmd == APP_CMD_CONFIG_CHANGED) resize(a, app);
    else android_game_lifecycle(a.game.get(), cmd);
}

} // namespace

void android_main(android_app* app) {
    game::stdout_to_logcat();
    // exe_dir — системный app_process: ассеты и сейвы задаёт оболочка в песочнице приложения.
    const std::string data = app->activity->internalDataPath;
    const std::string assets = data + "/assets";
    if (!game::unpack_apk_assets(app->activity->assetManager, assets))
        std::printf("[game] some assets were not unpacked to %s\n", assets.c_str());
    mobile::set_asset_dir(assets);
    platform::env_put("LIKENES_ASSETS", assets.c_str());
    platform::env_put("LIKENES_SAVE_DIR", data.c_str());
    platform::set_save_dir(data);
    const AndroidWindowSnapshot launch = android_window_read(app->activity);
    mobile::set_demo(launch.demo);
    mobile::set_args(launch.args);
    android_gamepad_begin(app->activity->vm);
    android_haptic_begin(app->activity->vm, app->activity->clazz);
    haptic::init();
    App a;
    app->userData = &a;
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;
    while (true) {
        int events;
        android_poll_source* src;
        const bool was_live = live(a);
        // Граница C-API: outData у ALooper_pollOnce — void**, looper кладёт туда android_poll_source*.
        while (ALooper_pollOnce(was_live ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&src)) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) {
                teardown(a); haptic::shutdown(); android_haptic_end(); android_gamepad_end(); return;
            }
            if (!was_live && live(a)) break;
        }
        if (!live(a)) continue;
        // Display.getRotation меняется и при 180°, когда CONFIG_CHANGED/размер прежние.
        if (a.frames++ % 30 == 0) android_window_request(app->activity);
        resize(a, app);
        const double t = now_s();
        a.game->frame(a.last > 0 ? t - a.last : 1.0 / 60);
        a.last = t;
    }
}
