#include "../plugin_api.h"

// Плагин со старым ABI: версия занижена намеренно, PLUGIN_EXPORT_ABI тут не годится. Хост обязан
// отбить загрузку до plugin_main — регистрация ниже не должна выполниться никогда.
extern "C" int32_t plugin_abi_version(void) { return PLUGIN_API_VERSION - 1; }

extern "C" void plugin_main(const HostApi* host) {
    host->log(host->ctx, "stale-abi plugin must never get here");
    host->register_ui_panel(host->ctx, "stale.panel", "Stale", nullptr);
}
