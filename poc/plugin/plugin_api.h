#pragma once
#include "sim.hpp"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_API_VERSION 1

typedef enum ExtKind {
    EXT_ECS_SYSTEM = 0,
    EXT_RENDER_PASS = 1,
    EXT_ASSET_CODEC = 2,
    EXT_INPUT_SOURCE = 3,
    EXT_AUDIO_BUS = 4,
    EXT_UI_PANEL = 5
} ExtKind;

typedef void (*SimSystemFn)(SimWorld*);
typedef int32_t (*AssetDecodeFn)(const uint8_t* in, int32_t in_len, uint8_t* out, int32_t out_cap);
typedef void (*OpaqueFn)(void);
typedef void (*UiDrawFn)(void* ui_ctx);

typedef struct HostApi {
    void* ctx;
    int32_t api_version;
    void (*register_ecs_system)(void* ctx, const char* id,
                                const char* const* after, int32_t n_after, SimSystemFn fn);
    void (*register_asset_codec)(void* ctx, const char* fourcc, AssetDecodeFn fn);
    void (*register_render_pass)(void* ctx, const char* id, OpaqueFn fn);
    void (*register_input_source)(void* ctx, const char* id, OpaqueFn fn);
    void (*register_audio_bus)(void* ctx, const char* id, OpaqueFn fn);
    void (*register_ui_panel)(void* ctx, const char* id, const char* title, UiDrawFn fn);
    void (*log)(void* ctx, const char* msg);
} HostApi;

typedef void (*PluginMainFn)(const HostApi*);
typedef int32_t (*PluginAbiFn)(void);

#ifdef __cplusplus
}
#endif

#define PLUGIN_EXPORT_ABI \
    extern "C" int32_t plugin_abi_version(void) { return PLUGIN_API_VERSION; }
