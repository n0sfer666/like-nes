#include "../plugin_api.h"

PLUGIN_EXPORT_ABI

extern "C" void wind_sys(SimWorld* w) {
    const fix32 dt = fix32::from_float(1.0 / 60.0);
    const fix32 wind = fix32::from_float(3.0);
    for (int i = 0; i < SimWorld::N; ++i) w->vx[i] = w->vx[i] + wind * dt;
}

extern "C" PLATFORM_EXPORT void plugin_main(const HostApi* h) {
    h->register_ecs_system(h->ctx, "wind", nullptr, 0, wind_sys);
    h->log(h->ctx, "wind registered");
}
