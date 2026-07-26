#include "plugin_test.hpp"

#include <cstdint>

#include "../game/backend_host.hpp"
#include "../plugin/registry.hpp"
#include "backend.hpp"
#include "delivery.hpp"
#include "plugin_backend.hpp"
#include "registry.hpp"
#include "tracker.hpp"

namespace ach_test {
namespace {

void drive(ach::Backend& b, ach::Registry& reg, ach::Tracker& tr, uint64_t expect_sent,
           const char* what) {
    ach::Delivery del(reg, tr, b);
    del.pump();
    check(del.stats().sent == expect_sent, what);
    del.shutdown();
}

// BackendHost выдаёт backend, живущий внутри dlopen-нутой библиотеки. Ветку перезагрузки в игре не
// достигает никто (init отвергает второй вызов), поэтому единственная её проверка — здесь: под
// -DPLUGIN_FORCE_DLCLOSE вызовы после подмены идут уже в новую библиотеку, и обращение к выгруженной
// поймает ASan.
void test_backend_host(const char* path) {
    ach::Registry reg;
    build(reg);
    game::BackendHost bh;

    ach::Backend* first = bh.load(path);
    check(first != nullptr, "BackendHost hands out a backend");
    if (first == nullptr) return;
    ach::Tracker tr(reg);
    tr.add_stat(ach::hash_key("stat_kills"), 10);
    drive(*first, reg, tr, 3, "backend behind BackendHost delivers");

    ach::Backend* again = bh.load(path);
    check(again != nullptr, "reload gives a live backend");
    if (again == nullptr) return;
    ach::Tracker tr2(reg);
    tr2.unlock(ach::hash_key("BOSS_DOWN"));
    drive(*again, reg, tr2, 1, "reloaded backend is live");

    check(bh.load("") == nullptr, "empty path unloads and yields no backend");
    check(bh.load(path) != nullptr, "loadable again after an empty path");
}

void test_no_plugin() {
    ::Registry host_reg;
    check(ach::find_backend(host_reg) == nullptr, "no backend without plugins");

    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    tr.add_stat(ach::hash_key("stat_kills"), 10);
    check(tr.unlocked(ach::hash_key("KILLER_10")), "unlocks work with no backend at all");
    check(tr.progress_hash() != 0, "local progress is the source of truth");
}

void test_null_api() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    ach::PluginBackend backend(nullptr);
    ach::Delivery del(reg, tr, backend);
    tr.add_stat(ach::hash_key("stat_kills"), 10);
    del.pump();
    check(del.stats().sent == 0, "null api never connects");
    check(!del.stats().dead, "null api is not a fatal backend");
    check(tr.unlocked(ach::hash_key("KILLER_10")), "local state unaffected");
}

} // namespace

void test_host_lifecycle(const char* plugin_path, bool with_reload) {
    if (with_reload) test_backend_host(plugin_path);
    test_no_plugin();
    test_null_api();
}

} // namespace ach_test
