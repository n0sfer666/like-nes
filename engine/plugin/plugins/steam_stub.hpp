#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Фейковый Steamworks: повторяет контракт ISteamUserStats (unlock/stat/store/callback),
// SDK не вендорится (спека #9 — инвентарь лицензий не трогается). Реальный SDK
// подключается сборкой с -DLIKE_NES_STEAM_SDK, шов адаптера один и тот же.
//   STEAM_STUB_INIT_FAIL=1   — SteamAPI_Init() падает (Steam не запущен)
//   STEAM_STUB_STORE_FAIL=N  — первые N StoreStats() возвращают false (сеть)
//   STEAM_STUB_REMOTE=A,B    — ачивки, уже открытые на сервисе (другая машина)
namespace steam_stub {

struct State {
    bool inited = false;
    int store_fail = 0;
    int stores = 0;
    bool stats_requested = false;
    bool remote_seeded = false;
    std::vector<std::string> achieved;
    std::vector<std::pair<std::string, int32_t>> stats;
};

inline State& state() {
    static State s;
    return s;
}

inline void split_env(const char* name, std::vector<std::string>& out) {
    const char* v = std::getenv(name);
    if (v == nullptr) return;
    std::string cur;
    for (const char* p = v;; ++p) {
        if (*p == ',' || *p == '\0') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
            if (*p == '\0') return;
            continue;
        }
        cur.push_back(*p);
    }
}

} // namespace steam_stub

class ISteamUserStats {
public:
    bool RequestCurrentStats() {
        steam_stub::State& s = steam_stub::state();
        if (!s.inited) return false;
        s.stats_requested = true;
        return true;
    }
    bool GetAchievement(const char* name, bool* achieved) {
        steam_stub::State& s = steam_stub::state();
        if (!s.inited || !s.stats_requested || achieved == nullptr) return false;
        *achieved = false;
        for (const std::string& a : s.achieved) {
            if (a == name) *achieved = true;
        }
        return true;
    }
    bool SetAchievement(const char* name) {
        steam_stub::State& s = steam_stub::state();
        if (!s.inited || !s.stats_requested) return false;
        for (const std::string& a : s.achieved) {
            if (a == name) return true;
        }
        s.achieved.push_back(name);
        return true;
    }
    bool SetStat(const char* name, int32_t data) {
        steam_stub::State& s = steam_stub::state();
        if (!s.inited || !s.stats_requested) return false;
        for (auto& kv : s.stats) {
            if (kv.first == name) {
                kv.second = data;
                return true;
            }
        }
        s.stats.emplace_back(name, data);
        return true;
    }
    bool StoreStats() {
        steam_stub::State& s = steam_stub::state();
        if (!s.inited) return false;
        ++s.stores;
        if (s.store_fail > 0) {
            --s.store_fail;
            return false;
        }
        return true;
    }
};

inline ISteamUserStats* SteamUserStats() {
    static ISteamUserStats api;
    return steam_stub::state().inited ? &api : nullptr;
}

inline bool SteamAPI_Init() {
    steam_stub::State& s = steam_stub::state();
    if (std::getenv("STEAM_STUB_INIT_FAIL") != nullptr) return false;
    const char* fail = std::getenv("STEAM_STUB_STORE_FAIL");
    s.store_fail = fail == nullptr ? 0 : std::atoi(fail);
    // Удалённые анлоки — состояние сервиса, а не сессии: досыпать их на каждый Init значило бы
    // размножать ключи при переподключении.
    if (!s.remote_seeded) {
        steam_stub::split_env("STEAM_STUB_REMOTE", s.achieved);
        s.remote_seeded = true;
    }
    s.inited = true;
    return true;
}

// Выключение гасит сессию, а не сервис: анлоки и статы переживают его, запрос статов — нет.
inline void SteamAPI_Shutdown() {
    steam_stub::State& s = steam_stub::state();
    s.inited = false;
    s.stats_requested = false;
}

inline void SteamAPI_RunCallbacks() {}
