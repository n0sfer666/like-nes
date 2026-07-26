#include "plugin_test.hpp"

#include <cstdio>
#include <cstdlib>

#include "../plugin/host.hpp"
#include "delivery.hpp"
#include "plugin_backend.hpp"
#include "registry.hpp"
#include "tracker.hpp"

namespace {

using ach_test::build;
using ach_test::check;

struct Loaded {
    ::Registry host_reg;
    PluginHost host{host_reg};
    const AchBackendApi* api = nullptr;
};

bool load(Loaded& l, const char* path) {
    if (!l.host.load_native(path)) return false;
    l.api = ach::find_backend(l.host_reg);
    return l.api != nullptr;
}

void test_seam(const char* path) {
    Loaded l;
    check(load(l, path), "plugin registered a backend");
    check(l.host_reg.count(EXT_ACHIEVEMENT_BACKEND) == 1, "one backend ext");
    check(ach::find_backend(l.host_reg, "fake") != nullptr, "backend addressable by id");
    check(ach::find_backend(l.host_reg, "steam") == nullptr, "unknown id not matched");
    if (l.api == nullptr) return;

    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    ach::PluginBackend backend(l.api);
    ach::Delivery del(reg, tr, backend);

    tr.add_stat(ach::hash_key("stat_kills"), 10);
    del.pump();
    check(del.pending() == 0, "queue drained");
    check(del.stats().sent == 3, "two unlocks and one stat sent");
    check(del.stats().commits == 1, "committed once");

    del.pump();
    check(del.stats().sent == 3, "idle pump sends nothing");

    tr.unlock(ach::hash_key("BOSS_DOWN"));
    del.pump();
    check(del.stats().sent == 4, "later unlock delivered");

    del.reconcile();
    check(del.stats().reconciled == 0, "empty remote set changes nothing");

    del.shutdown();
    l.host.unload(path);
    check(l.host_reg.count(EXT_ACHIEVEMENT_BACKEND) == 0, "unload drops the ext");
}

int env_int(const char* name) {
    const char* v = std::getenv(name);
    return v == nullptr ? 0 : std::atoi(v);
}

// Ручки ach_fake читаются плагином на загрузке, поэтому сценарий задаётся окружением процесса:
// один прогон — одна неисправность, и путь идёт через настоящий C-ABI, а не через тест-дубль.
void test_knobs(const char* path) {
    Loaded l;
    check(load(l, path), "plugin registered a backend");
    if (l.api == nullptr) return;

    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    ach::PluginBackend backend(l.api);
    ach::Delivery del(reg, tr, backend);

    const int offline = env_int("ACH_FAKE_OFFLINE");
    const int retry = env_int("ACH_FAKE_RETRY");
    const bool fatal = env_int("ACH_FAKE_FATAL") != 0;
    const char* remote = std::getenv("ACH_FAKE_REMOTE");
    const char* late = std::getenv("ACH_FAKE_REMOTE_LATE");

    tr.add_stat(ach::hash_key("stat_kills"), 10);
    for (int i = 0; i <= offline + retry; ++i) del.pump();

    if (fatal) {
        check(del.stats().dead, "fatal backend marked dead");
        check(del.stats().sent == 0 && del.pending() == 0, "queue dropped, nothing sent");
    } else {
        check(del.stats().sent == 3, "backlog delivered once the backend recovers");
        check(del.stats().retried == static_cast<uint64_t>(retry), "every retry accounted for");
        check(del.pending() == 0, "queue drained");
    }
    check(tr.unlocked(ach::hash_key("KILLER_10")), "local progress survives any backend fault");

    del.reconcile();
    if (remote != nullptr && !fatal) {
        check(del.stats().reconciled == 1, "remote unlock adopted through the C ABI");
        check(tr.unlocked(ach::hash_key("BOSS_DOWN")), "adopted unlock is local now");
    }

    // Сверка периодическая: ключ, открытый на сервисе уже после первого опроса, обязан доехать
    // следующим. Однократный опрос на старте этот сценарий проваливает — тут он и пинится.
    const uint64_t reconciled = del.stats().reconciled;
    const uint64_t sent = del.stats().sent;
    del.reconcile();
    if (late != nullptr && !fatal) {
        check(del.stats().reconciled == reconciled + 1, "late remote unlock adopted");
        check(tr.unlocked(ach::hash_key(late)), "late unlock is local now");
    } else {
        check(del.stats().reconciled == reconciled, "repeated poll adopts nothing new");
    }
    del.pump();
    check(del.stats().sent == sent, "adopted remote unlock is not echoed back");

    del.shutdown();
    l.host.unload(path);
    check(l.host_reg.count(EXT_ACHIEVEMENT_BACKEND) == 0, "unload drops the ext");
}

bool any_knob() {
    return std::getenv("ACH_FAKE_OFFLINE") != nullptr || std::getenv("ACH_FAKE_RETRY") != nullptr ||
           std::getenv("ACH_FAKE_FATAL") != nullptr || std::getenv("ACH_FAKE_REMOTE") != nullptr ||
           std::getenv("ACH_FAKE_REMOTE_LATE") != nullptr;
}

// Гейт ABI: плагин, сообщающий PLUGIN_API_VERSION-1, обязан быть отбит ДО plugin_main.
void test_stale_abi(const char* path) {
    Loaded l;
    check(!l.host.load_native(path), "stale ABI plugin rejected");
    check(l.host_reg.count(EXT_UI_PANEL) == 0, "rejected plugin registered nothing");
    check(ach::find_backend(l.host_reg) == nullptr, "no backend from a rejected plugin");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: ach_plugin_test <plugin-path> [stale-abi-plugin]\n");
        return 2;
    }
    std::printf("achievements plugin seam\n");
    const bool knobs = any_knob();
    if (knobs) {
        test_knobs(argv[1]);
    } else {
        test_seam(argv[1]);
    }
    // Ручки меняют ответы бэкенда, а сценарий жизненного цикла считает отправки — гоняем его только
    // на чистом окружении, где счётчики предсказуемы.
    ach_test::test_host_lifecycle(argv[1], !knobs);
    if (argc >= 3) test_stale_abi(argv[2]);
    std::printf(ach_test::failures == 0 ? "PASS\n" : "FAIL\n");
    return ach_test::failures == 0 ? 0 : 1;
}
