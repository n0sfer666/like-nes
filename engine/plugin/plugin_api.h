#pragma once
#include "platform_export.h"
#include "sim.hpp"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_API_VERSION 2

typedef enum ExtKind {
    EXT_ECS_SYSTEM = 0,
    EXT_RENDER_PASS = 1,
    EXT_ASSET_CODEC = 2,
    EXT_INPUT_SOURCE = 3,
    EXT_AUDIO_BUS = 4,
    EXT_UI_PANEL = 5,
    EXT_ACHIEVEMENT_BACKEND = 6,
    // Сентинель физически последний: новая точка расширения двигает счётчик сама. Ручной макрос
    // отставал бы молча — add_named отвергал бы вид, count() врал бы 0, сборка проходила бы.
    EXT_KIND_LAST
} ExtKind;

#define EXT_KIND_COUNT EXT_KIND_LAST

typedef void (*SimSystemFn)(SimWorld*);
typedef int32_t (*AssetDecodeFn)(const uint8_t* in, int32_t in_len, uint8_t* out, int32_t out_cap);
typedef void (*OpaqueFn)(void);
typedef void (*UiDrawFn)(void* ui_ctx);

typedef enum AchSend {
    ACH_SEND_OK = 0,
    ACH_SEND_RETRY = 1,
    ACH_SEND_FATAL = 2
} AchSend;

/* Бэкенд достижений: доставка наружу (Steam и подобные). Асинхронный по природе —
   вызывается ВНЕ тика, детерминизм sim от него не зависит. Структура принадлежит
   плагину и обязана пережить выгрузку регистрации (статическое хранение). */
typedef struct AchBackendApi {
    void* self;
    int32_t (*begin)(void* self);                                   /* 0 = недоступен */
    void (*declare)(void* self, const char* key);                   /* каталог после connect */
    int32_t (*unlock)(void* self, const char* key);                 /* AchSend */
    int32_t (*set_stat)(void* self, const char* key, uint64_t value);
    int32_t (*commit)(void* self);
    /* <0 = неизвестно. Вызывается периодически (хост сверяется по ходу сессии), поэтому обязан
       отдавать ПОЛНЫЙ текущий набор удалённых анлоков, а не дельту с прошлого раза: диффит хост,
       по своим локальным флагам. Записанные ключи принадлежат плагину и обязаны жить до следующего
       вызова: хост читает их после возврата и копий не делает. */
    int32_t (*poll_remote)(void* self, const char** out_keys, int32_t cap);
    void (*end)(void* self);
} AchBackendApi;

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
    void (*register_achievement_backend)(void* ctx, const char* id, const AchBackendApi* backend);
    void (*log)(void* ctx, const char* msg);
} HostApi;

typedef void (*PluginMainFn)(const HostApi*);
typedef int32_t (*PluginAbiFn)(void);

#ifdef __cplusplus
}
#endif

#define PLUGIN_EXPORT_ABI \
    extern "C" PLATFORM_EXPORT int32_t plugin_abi_version(void) { return PLUGIN_API_VERSION; }
