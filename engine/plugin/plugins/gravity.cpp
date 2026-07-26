#include "../plugin_api.h"

PLUGIN_EXPORT_ABI

extern "C" void gravity_sys(SimWorld* w) {
    const fix32 dt = fix32::from_float(1.0 / 60.0);
    const fix32 g = fix32::from_float(9.8);
    for (int i = 0; i < SimWorld::N; ++i) w->vy[i] = w->vy[i] + g * dt;
}

extern "C" void plugin_main(const HostApi* h) {
    h->register_ecs_system(h->ctx, "gravity", nullptr, 0, gravity_sys);
    h->log(h->ctx, "gravity registered");
}
