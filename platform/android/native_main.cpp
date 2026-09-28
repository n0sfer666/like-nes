#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/native_window.h>

#include <webgpu/webgpu.h>

#include "apk_assets.hpp"
#include "gpu.hpp"
#include "mobile_game.hpp"
#include "platform_env.hpp"
#include "stdout_logcat.hpp"

using namespace game;

namespace {

struct App {
    GpuContext gpu;
    MobileGame game;
    WGPUSurface surface = nullptr;
    bool ready = false;
    float vw = 0, vh = 0;
};

void release_gpu(App& a) {
    if (a.surface) { wgpuSurfaceRelease(a.surface); a.surface = nullptr; }
    a.gpu.shutdown();
}

// Отказ освобождает всё поднятое: следующий INIT_WINDOW начнёт init заново, а живые instance и
// поверхность на том же ANativeWindow иначе копились бы с каждым кругом.
void init(App& a, ANativeWindow* win) {
    if (a.ready) return;
    a.gpu.instance = wgpuCreateInstance(nullptr);
    WGPUSurfaceDescriptorFromAndroidNativeWindow nw = {};
    nw.chain.sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow;
    nw.window = win;
    WGPUSurfaceDescriptor sd = {};
    sd.nextInChain = &nw.chain;
    a.surface = wgpuInstanceCreateSurface(a.gpu.instance, &sd);
    if (!a.gpu.init(a.surface)) { release_gpu(a); return; }
    const uint32_t w = static_cast<uint32_t>(ANativeWindow_getWidth(win));
    const uint32_t h = static_cast<uint32_t>(ANativeWindow_getHeight(win));
    a.vw = static_cast<float>(w); a.vh = static_cast<float>(h);
    if (!a.game.init(a.gpu, a.surface, w, h)) { release_gpu(a); return; }
    a.ready = true;
}

void teardown(App& a) {
    if (!a.ready) return;
    a.ready = false;
    a.game.shutdown();
    release_gpu(a);
}

void dispatch(App& a, AInputEvent* ev, int idx, MobileGame::Touch phase) {
    a.game.pointer(AMotionEvent_getPointerId(ev, idx), phase,
                   AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx), a.vw, a.vh);
}

int32_t on_input(android_app* app, AInputEvent* ev) {
    App& a = *static_cast<App*>(app->userData);
    if (!a.ready || AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int32_t raw = AMotionEvent_getAction(ev);
    const int32_t action = raw & AMOTION_EVENT_ACTION_MASK;
    const int32_t idx = (raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                        >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            dispatch(a, ev, idx, MobileGame::Touch::Down);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i)
                dispatch(a, ev, static_cast<int>(i), MobileGame::Touch::Move);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            dispatch(a, ev, idx, MobileGame::Touch::Up);
            break;
        case AMOTION_EVENT_ACTION_CANCEL:   // pointer-index не кодируется → сбросить все касания
            a.game.cancel();
            break;
    }
    return 1;
}

void on_cmd(android_app* app, int32_t cmd) {
    App& a = *static_cast<App*>(app->userData);
    if (cmd == APP_CMD_INIT_WINDOW && app->window) init(a, app->window);
    else if (cmd == APP_CMD_TERM_WINDOW) teardown(a);
    else if (cmd == APP_CMD_PAUSE) a.game.suspend();
}

} // namespace

void android_main(android_app* app) {
    stdout_to_logcat();
    // exe_dir у приложения — app_process системы, а HOME не задан: пути к ассетам и сейву игра
    // получает теми же ручками окружения, что и десктоп, только указывают они в песочницу.
    const std::string data = app->activity->internalDataPath;
    const std::string assets = data + "/assets";
    if (unpack_apk_assets(app->activity->assetManager, assets))
        platform::env_put("LIKENES_ASSETS", assets.c_str());
    platform::env_put("LIKENES_SAVE_DIR", data.c_str());
    App a;
    app->userData = &a;
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;
    while (true) {
        int events;
        android_poll_source* src;
        // Граница C-API: outData у ALooper_pollOnce — void**, looper кладёт туда android_poll_source*.
        while (ALooper_pollOnce(a.ready ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&src)) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { teardown(a); return; }
        }
        if (a.ready) a.game.frame(a.surface);
    }
}
