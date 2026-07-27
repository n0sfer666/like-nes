#include <cstdio>
#include <cstdlib>
#include <string>

#include "../plugin/host.hpp"
#include "delivery.hpp"
#include "platform_args.hpp"
#include "platform_env.hpp"
#include "plugin_backend.hpp"
#include "registry.hpp"
#include "tracker.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

using AchievedFn = int32_t (*)(const char*);
using StatFn = int32_t (*)(const char*);
using StoresFn = int32_t (*)();
using GrantFn = void (*)(const char*);

// Заглушка опрашивается через ТОТ ЖЕ загруженный модуль, что держит хост: открыть путь вторым
// Module'ом переносимо нельзя — на Windows это отдельная копия со своей статикой, и вызовы,
// записанные бэкендом, инспектору просто не видны (см. platform_module.hpp).
struct Stub {
    AchievedFn achieved = nullptr;
    StatFn stat = nullptr;
    StoresFn stores = nullptr;
    GrantFn grant = nullptr;
};

Stub open_stub(const PluginHost& host, const char* path) {
    Stub s;
    s.achieved = reinterpret_cast<AchievedFn>(host.symbol(path, "steam_stub_achieved"));
    s.stat = reinterpret_cast<StatFn>(host.symbol(path, "steam_stub_stat"));
    s.stores = reinterpret_cast<StoresFn>(host.symbol(path, "steam_stub_stores"));
    s.grant = reinterpret_cast<GrantFn>(host.symbol(path, "steam_stub_grant"));
    return s;
}

void build(ach::Registry& reg) {
    reg.define({"FIRST_BLOOD", "First Blood", "", ach::Kind::Progress, "stat_kills", 1, 0});
    reg.define({"KILLER_10", "Killer", "", ach::Kind::Progress, "stat_kills", 10, 0});
    reg.define({"BOSS_DOWN", "Boss Down", "", ach::Kind::Boolean, nullptr, 0, 0});
    reg.define({"FLAWLESS", "Flawless", "", ach::Kind::Boolean, nullptr, 0, 0});
}

void run(const char* path) {
    ::Registry host_reg;
    PluginHost host(host_reg);
    check(host.load_native(path), "steam plugin loaded");
    const AchBackendApi* api = ach::find_backend(host_reg, "steam");
    check(api != nullptr, "steam backend registered");
    if (api == nullptr) return;

    const Stub stub = open_stub(host, path);
    check(stub.achieved != nullptr && stub.stat != nullptr && stub.stores != nullptr,
          "stub inspection symbols found");
    if (stub.achieved == nullptr || stub.stat == nullptr || stub.stores == nullptr) return;

    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    ach::PluginBackend backend(api);
    ach::Delivery del(reg, tr, backend);

    tr.add_stat(ach::hash_key("stat_kills"), 10);
    del.pump();
    check(del.stats().connected, "SteamAPI_Init + RequestCurrentStats succeeded");
    check(stub.achieved("FIRST_BLOOD") == 1, "SetAchievement called for first blood");
    check(stub.achieved("KILLER_10") == 1, "SetAchievement called for killer");
    check(stub.stat("stat_kills") == 10, "SetStat mirrored the counter");
    check(stub.stores() >= 1, "StoreStats committed");

    del.reconcile();
    check(tr.unlocked(ach::hash_key("BOSS_DOWN")) == platform::env_has("STEAM_STUB_REMOTE"),
          "remote unlock adopted only when the service has it");

    const uint64_t sent = del.stats().sent;
    del.pump();
    check(del.stats().sent == sent, "no echo back to Steam after reconcile");

    // Сверка периодическая (live.cpp зовёт pump раз в 60 тиков): ачивка, выданная сервисом уже
    // после первого опроса, обязана подхватиться следующим. Однократный опрос на старте этот
    // сценарий проваливает, поэтому пинится он именно тут.
    const uint64_t reconciled = del.stats().reconciled;
    const int32_t stores = stub.stores();
    check(stub.grant != nullptr, "stub exposes a mid-session grant");
    if (stub.grant == nullptr) return;
    stub.grant("FLAWLESS");
    del.reconcile();
    check(del.stats().reconciled == reconciled + 1, "mid-session remote unlock adopted");
    check(tr.unlocked(ach::hash_key("FLAWLESS")), "adopted unlock is local now");
    del.pump();
    check(del.stats().sent == sent, "adopted unlock is not echoed back to Steam");
    check(stub.stores() == stores, "no StoreStats for what Steam already knows");

    del.shutdown();
    host.unload(path);
    check(host_reg.count(EXT_ACHIEVEMENT_BACKEND) == 0, "unload drops the steam backend");
}

void test_store_retry(const char* path) {
    ::Registry host_reg;
    PluginHost host(host_reg);
    if (!host.load_native(path)) return;
    const AchBackendApi* api = ach::find_backend(host_reg, "steam");
    if (api == nullptr) return;

    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    ach::PluginBackend backend(api);
    ach::Delivery del(reg, tr, backend);
    tr.unlock(ach::hash_key("BOSS_DOWN"));
    del.pump();
    check(!del.stats().dead, "failed StoreStats is a retry, not a death");
    check(tr.unlocked(ach::hash_key("BOSS_DOWN")), "local progress intact regardless of Steam");
    del.shutdown();
    host.unload(path);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 2) {
        std::printf("usage: ach_steam_test <steam-plugin-path>\n");
        return 2;
    }
    std::printf("achievements steam adapter (contract stub)\n");
    if (platform::env_has("STEAM_STUB_STORE_FAIL")) {
        test_store_retry(argv[1]);
    } else if (platform::env_has("STEAM_STUB_INIT_FAIL")) {
        ::Registry host_reg;
        PluginHost host(host_reg);
        check(host.load_native(argv[1]), "plugin loads without Steam running");
        const AchBackendApi* api = ach::find_backend(host_reg, "steam");
        check(api != nullptr, "steam backend registered without Steam running");
        ach::Registry reg;
        build(reg);
        ach::Tracker tr(reg);
        ach::PluginBackend backend(api);
        ach::Delivery del(reg, tr, backend);
        tr.unlock(ach::hash_key("BOSS_DOWN"));
        del.pump();
        check(!del.stats().connected && del.stats().sent == 0, "no Steam, no delivery");
        check(tr.unlocked(ach::hash_key("BOSS_DOWN")), "game keeps its own progress");
        del.shutdown();
    } else {
        run(argv[1]);
    }
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
