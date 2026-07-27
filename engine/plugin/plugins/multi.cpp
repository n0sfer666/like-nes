#include "../plugin_api.h"

PLUGIN_EXPORT_ABI

extern "C" void demo_noop(void) {}
extern "C" void demo_panel_draw(void*) {}

extern "C" PLATFORM_EXPORT void plugin_main(const HostApi* h) {
    h->register_render_pass(h->ctx, "demo.pass", demo_noop);
    h->register_input_source(h->ctx, "demo.input", demo_noop);
    h->register_audio_bus(h->ctx, "demo.bus", demo_noop);
    h->register_ui_panel(h->ctx, "demo.panel", "Demo Panel", demo_panel_draw);
    h->log(h->ctx, "multi ext-points registered (render/input/audio/ui)");
}
