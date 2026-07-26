#include "../plugin_api.h"

PLUGIN_EXPORT_ABI

extern "C" void crash_sys(SimWorld*) {
    volatile int* p = nullptr;
    *p = 42;
}

extern "C" void plugin_main(const HostApi* h) {
    h->register_ecs_system(h->ctx, "zzz_crash", nullptr, 0, crash_sys);
    h->log(h->ctx, "zzz_crash registered (will fault on probe)");
}
