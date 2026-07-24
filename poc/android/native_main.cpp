#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/native_window.h>

#include <webgpu/webgpu.h>

#include <memory>

#include "action_map.hpp"
#include "art.hpp"
#include "batch.hpp"
#include "codes.hpp"
#include "draw.hpp"
#include "engine.hpp"
#include "gpu.hpp"
#include "sim.hpp"
#include "stick.hpp"
#include "world.hpp"

using namespace game;

namespace {

struct App {
    GpuContext gpu;
    SpriteBatch batch;
    Atlas atlas;
    flecs::world world;
    game::GameState gs;
    input::ActionMap map;
    std::unique_ptr<input::InputEngine> engine;
    WGPUSurface surface = nullptr;
    bool ready = false;
    bool sim_ready = false;
    uint32_t tick = 0;
    uint64_t seq = 0;
    float ox = 0, oy = 0;
    bool tracking = false;
};

void emit_axis(App& a, fix32 x, fix32 y) {
    a.engine->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                    (uint16_t)input::code::LX, x.raw, a.seq++});
    a.engine->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                    (uint16_t)input::code::LY, y.raw, a.seq++});
}

void init(App& a, ANativeWindow* win) {
    if (a.ready) return;
    a.gpu.instance = wgpuCreateInstance(nullptr);
    WGPUSurfaceDescriptorFromAndroidNativeWindow nw = {};
    nw.chain.sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow;
    nw.window = win;
    WGPUSurfaceDescriptor sd = {};
    sd.nextInChain = &nw.chain;
    a.surface = wgpuInstanceCreateSurface(a.gpu.instance, &sd);
    if (!a.gpu.init(a.surface)) {
        wgpuSurfaceRelease(a.surface);
        a.surface = nullptr;
        return;
    }

    const uint32_t w = (uint32_t)ANativeWindow_getWidth(win);
    const uint32_t h = (uint32_t)ANativeWindow_getHeight(win);
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(a.surface, a.gpu.adapter, &caps);
    WGPUTextureFormat fmt = caps.formatCount ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = a.gpu.device; cfg.format = fmt;
    cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
    cfg.width = w; cfg.height = h; cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(a.surface, &cfg);
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    a.atlas = build_atlas();
    a.batch.init(a.gpu.device, a.gpu.queue, fmt, a.atlas);
    const double sa = (double)w / h, wa = (double)VIEW_W / VIEW_H;
    a.batch.set_viewport(sa >= wa ? (uint32_t)(VIEW_H * sa) : VIEW_W,
                         sa >= wa ? VIEW_H : (uint32_t)(VIEW_W / sa));
    if (!a.sim_ready) {
        spawn(a.world, a.gs);
        a.gs.phase = game::PH_Play;   // mobile: нет A_Fire-ввода → пропускаем intro (S10)
        a.map = make_map();
        a.engine.reset(new input::InputEngine(a.map));
        a.engine->post({input::RawKind::DeviceConnected, input::DeviceKind::Gamepad, 0, 0, 0, a.seq++});
        a.sim_ready = true;
    }
    a.ready = true;
}

void teardown(App& a) {
    if (!a.ready) return;
    a.ready = false;
    a.tracking = false;
    a.batch.shutdown();
    if (a.surface) { wgpuSurfaceRelease(a.surface); a.surface = nullptr; }
    a.gpu.shutdown();
}

void render(App& a) {
    if (!a.ready) return;
    const input::InputFrame& f = a.engine->begin_tick(a.tick++, 0);
    step(a.world, a.gs, f, fix32::from_float(1.0 / 60));

    WGPUSurfaceTexture st = {};
    wgpuSurfaceGetCurrentTexture(a.surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        if (st.texture) wgpuTextureRelease(st.texture);
        return;
    }
    WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
    a.batch.begin();
    push_scene(a.batch, a.world, a.atlas);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(a.gpu.device, nullptr);
    WGPURenderPassEncoder pass = begin_clear(enc, view);
    a.batch.flush(pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(a.gpu.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuSurfacePresent(a.surface);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);
}

int32_t on_input(android_app* app, AInputEvent* ev) {
    App& a = *(App*)app->userData;
    if (!a.ready || AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int32_t action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
    const float x = AMotionEvent_getX(ev, 0), y = AMotionEvent_getY(ev, 0);
    if (action == AMOTION_EVENT_ACTION_DOWN) { a.ox = x; a.oy = y; a.tracking = true; }
    else if (action == AMOTION_EVENT_ACTION_MOVE && a.tracking) {
        const StickAxis s = stick_axis(x - a.ox, y - a.oy, 120.0);
        emit_axis(a, s.x, s.y);
    } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
        a.tracking = false; emit_axis(a, fix32{}, fix32{});
    }
    return 1;
}

void on_cmd(android_app* app, int32_t cmd) {
    App& a = *(App*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW && app->window) init(a, app->window);
    else if (cmd == APP_CMD_TERM_WINDOW) teardown(a);
}

} // namespace

void android_main(android_app* app) {
    App a;
    app->userData = &a;
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;
    while (true) {
        int events;
        android_poll_source* src;
        while (ALooper_pollOnce(a.ready ? 0 : -1, nullptr, &events, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { teardown(a); return; }
        }
        render(a);
    }
}
