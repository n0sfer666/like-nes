#include <cstdio>

#include "delivery.hpp"
#include "delivery_fake.hpp"

namespace {

// begin/end симметричны: штатное разрушение обязано закрыть сессию бэкенда.
void test_shutdown_closes_the_session() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    FakeBackend b;
    {
        ach::Delivery del(reg, tr, b);
        tr.unlock(ach::hash_key("BOSS_DOWN"));
        del.pump();
        check(!b.ended, "session stays open while delivery lives");
    }
    check(b.ended, "session closed on destruction");

    FakeBackend idle;
    { ach::Delivery del(reg, tr, idle); }
    check(!idle.ended, "backend that never connected is not shut down");
}

// Явное закрытие — часть контракта выгрузки плагина: после него доставка молчит, а деструктор
// не зовёт end() второй раз по бэкенду, который уже может лежать в выгруженной библиотеке.
void test_explicit_shutdown_is_final() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    FakeBackend b;
    {
        ach::Delivery del(reg, tr, b);
        del.pump();
        del.shutdown();
        check(b.ended, "explicit shutdown closes the session");
        const uint64_t sent = del.stats().sent;

        b.ended = false;
        b.remote.push_back("BOSS_DOWN");
        tr.add_stat(ach::hash_key("stat_kills"), 10);
        del.pump();
        del.reconcile();
        check(del.stats().sent == sent, "closed delivery never touches the backend again");
        check(del.stats().reconciled == 0, "closed delivery does not poll the backend");
    }
    check(!b.ended, "destruction after an explicit shutdown does not close it twice");
}

} // namespace

int main() {
    std::printf("achievements delivery lifecycle\n");
    test_shutdown_closes_the_session();
    test_explicit_shutdown_is_final();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
