#include "../plugin_api.h"

#include <string>
#include <vector>

#ifdef LIKE_NES_STEAM_SDK
#include <steam/steam_api.h>
#else
#include "steam_stub.hpp"
#endif

// Адаптер Steam: плагин на точке расширения EXT_ACHIEVEMENT_BACKEND. Движок про Steam
// не знает; отсутствие плагина = игра работает на локальном прогрессе.
namespace {

struct Steam {
    bool inited = false;
    std::vector<std::string> keys;
    std::vector<std::string> polled_keys;
};

Steam g_steam;

int32_t steam_begin(void* self) {
    Steam* s = static_cast<Steam*>(self);
    if (s->inited) return 1;
    if (!SteamAPI_Init()) return 0;
    if (SteamUserStats() == nullptr || !SteamUserStats()->RequestCurrentStats()) {
        SteamAPI_Shutdown();
        return 0;
    }
    s->inited = true;
    return 1;
}

void steam_declare(void* self, const char* key) {
    Steam* s = static_cast<Steam*>(self);
    for (const std::string& k : s->keys) {
        if (k == key) return;
    }
    s->keys.emplace_back(key);
}

int32_t steam_unlock(void*, const char* key) {
    if (SteamUserStats() == nullptr) return ACH_SEND_RETRY;
    return SteamUserStats()->SetAchievement(key) ? ACH_SEND_OK : ACH_SEND_RETRY;
}

int32_t steam_set_stat(void*, const char* key, uint64_t value) {
    if (SteamUserStats() == nullptr) return ACH_SEND_RETRY;
    const int32_t clamped =
        value > 0x7fffffffull ? 0x7fffffff : static_cast<int32_t>(value);
    return SteamUserStats()->SetStat(key, clamped) ? ACH_SEND_OK : ACH_SEND_RETRY;
}

int32_t steam_commit(void*) {
    if (SteamUserStats() == nullptr) return ACH_SEND_RETRY;
    SteamAPI_RunCallbacks();
    return SteamUserStats()->StoreStats() ? ACH_SEND_OK : ACH_SEND_RETRY;
}

int32_t steam_poll_remote(void* self, const char** out_keys, int32_t cap) {
    Steam* s = static_cast<Steam*>(self);
    if (!s->inited || SteamUserStats() == nullptr) return -1;
    SteamAPI_RunCallbacks();
    // Ключи отдаются из отдельного снапшота, а не из s->keys: declare() продолжает пушить в тот же
    // вектор, и реаллокация оборвала бы указатели, которые контракт обязывает держать валидными
    // до следующего вызова.
    s->polled_keys.clear();
    for (const std::string& k : s->keys) {
        if (static_cast<int32_t>(s->polled_keys.size()) >= cap) break;
        bool achieved = false;
        if (!SteamUserStats()->GetAchievement(k.c_str(), &achieved)) return -1;
        if (achieved) s->polled_keys.push_back(k);
    }
    int32_t n = 0;
    for (const std::string& k : s->polled_keys) out_keys[n++] = k.c_str();
    return n;
}

void steam_end(void* self) {
    Steam* s = static_cast<Steam*>(self);
    if (!s->inited) return;
    SteamAPI_Shutdown();
    s->inited = false;
}

AchBackendApi g_api = {.self = &g_steam,
                       .begin = steam_begin,
                       .declare = steam_declare,
                       .unlock = steam_unlock,
                       .set_stat = steam_set_stat,
                       .commit = steam_commit,
                       .poll_remote = steam_poll_remote,
                       .end = steam_end};

} // namespace

PLUGIN_EXPORT_ABI

extern "C" PLATFORM_EXPORT void plugin_main(const HostApi* host) {
    host->register_achievement_backend(host->ctx, "steam", &g_api);
#ifdef LIKE_NES_STEAM_SDK
    host->log(host->ctx, "steam backend registered (real Steamworks SDK)");
#else
    host->log(host->ctx, "steam backend registered (contract stub, no SDK)");
#endif
}

#ifndef LIKE_NES_STEAM_SDK
extern "C" PLATFORM_EXPORT int32_t steam_stub_achieved(const char* key) {
    for (const std::string& a : steam_stub::state().achieved) {
        if (a == key) return 1;
    }
    return 0;
}

extern "C" PLATFORM_EXPORT int32_t steam_stub_stat(const char* key) {
    for (const auto& kv : steam_stub::state().stats) {
        if (kv.first == key) return kv.second;
    }
    return -1;
}

extern "C" PLATFORM_EXPORT int32_t steam_stub_stores() { return steam_stub::state().stores; }

// Выдать ачивку со стороны сервиса посреди сессии — оверлей, вторая машина. Без этого сверку
// нечем отличить от однократного опроса на старте.
extern "C" PLATFORM_EXPORT void steam_stub_grant(const char* key) {
    steam_stub::State& s = steam_stub::state();
    for (const std::string& a : s.achieved) {
        if (a == key) return;
    }
    s.achieved.emplace_back(key);
}
#endif
