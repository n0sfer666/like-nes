#include <android_native_app_glue.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/native_window.h>

#include <webgpu/webgpu.h>

#include <cstdio>
#include <ctime>
#include <memory>

#include "android_host.hpp"
#include "apk_assets.hpp"
#include "gpu.hpp"
#include "mobile_app.hpp"
#include "platform_env.hpp"
#include "stdout_logcat.hpp"

namespace {

struct App {
    GpuContext gpu;
    std::unique_ptr<mobile::Game> game;
    WGPUSurface surface = nullptr;
    uint32_t fb_w = 0, fb_h = 0;
    float scale = 1;
    double last = 0;
};

double now_s() {
    timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

// Касания приходят в пикселях окна, а игра ждёт точки: плотность 160 dpi — одна точка на пиксель.
float density_scale(AConfiguration* config) {
    const int32_t d = config ? AConfiguration_getDensity(config) : 0;
    if (d <= 0 || d == ACONFIGURATION_DENSITY_NONE) return 1;
    return static_cast<float>(d) / static_cast<float>(ACONFIGURATION_DENSITY_MEDIUM);
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

mobile::SafeArea safe_area(const App& a) { return {0, 0, 0, 0, a.scale}; }

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
    a.scale = density_scale(app->config);
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
}

void detach(App& a) {
    if (a.game) a.game->detach_surface();
    release_surface(a);
}

// Смена плотности (перенос на другой экран, настройка размера шрифтов) меняет масштаб касаний и
// при прежнем размере окна, поэтому resize уходит игре и тогда.
void resize(App& a, android_app* app) {
    if (!a.game || !a.surface || !app->window) return;
    const int32_t w = ANativeWindow_getWidth(app->window), h = ANativeWindow_getHeight(app->window);
    const float scale = density_scale(app->config);
    if (w <= 0 || h <= 0) return;
    if (static_cast<uint32_t>(w) == a.fb_w && static_cast<uint32_t>(h) == a.fb_h && scale == a.scale) return;
    a.fb_w = static_cast<uint32_t>(w); a.fb_h = static_cast<uint32_t>(h); a.scale = scale;
    a.game->resize(a.fb_w, a.fb_h, safe_area(a));
}

bool live(const App& a) { return a.game && a.surface && a.game->wants_frames(); }

void dispatch(App& a, AInputEvent* ev, size_t idx, mobile::Touch phase) {
    a.game->pointer(AMotionEvent_getPointerId(ev, idx), phase,
                    AMotionEvent_getX(ev, idx) / a.scale, AMotionEvent_getY(ev, idx) / a.scale);
}

int32_t on_input(android_app* app, AInputEvent* ev) {
    App& a = *static_cast<App*>(app->userData);
    if (!a.game || !a.surface || AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int32_t raw = AMotionEvent_getAction(ev);
    const int32_t action = raw & AMOTION_EVENT_ACTION_MASK;
    const size_t idx = static_cast<size_t>((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                           >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            dispatch(a, ev, idx, mobile::Touch::Down);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i)
                dispatch(a, ev, i, mobile::Touch::Move);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            dispatch(a, ev, idx, mobile::Touch::Up);
            break;
        case AMOTION_EVENT_ACTION_CANCEL:   // pointer-index не кодируется → сбросить все касания
            a.game->cancel();
            break;
    }
    return 1;
}

void on_cmd(android_app* app, int32_t cmd) {
    App& a = *static_cast<App*>(app->userData);
    if (cmd == APP_CMD_INIT_WINDOW && app->window) attach(a, app);
    else if (cmd == APP_CMD_TERM_WINDOW) detach(a);
    else if (cmd == APP_CMD_WINDOW_RESIZED || cmd == APP_CMD_CONFIG_CHANGED) resize(a, app);
    else if (cmd == APP_CMD_PAUSE && a.game) a.game->suspend();
    else if (cmd == APP_CMD_RESUME && a.game) a.game->resume();
}

} // namespace

void android_main(android_app* app) {
    game::stdout_to_logcat();
    // exe_dir у приложения — app_process системы, а HOME не задан: пути к ассетам и сейву игра
    // получает теми же ручками окружения, что и десктоп, только указывают они в песочницу.
    const std::string data = app->activity->internalDataPath;
    const std::string assets = data + "/assets";
    if (!game::unpack_apk_assets(app->activity->assetManager, assets))
        std::printf("[game] some assets were not unpacked to %s\n", assets.c_str());
    mobile::set_asset_dir(assets);
    platform::env_put("LIKENES_ASSETS", assets.c_str());
    platform::env_put("LIKENES_SAVE_DIR", data.c_str());
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
            if (app->destroyRequested) { teardown(a); return; }
            if (!was_live && live(a)) break;
        }
        if (!live(a)) continue;
        const double t = now_s();
        a.game->frame(a.last > 0 ? t - a.last : 1.0 / 60);
        a.last = t;
    }
}
