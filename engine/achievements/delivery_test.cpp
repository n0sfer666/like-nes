#include <cstdio>

#include "delivery.hpp"
#include "delivery_fake.hpp"

namespace {

void test_offline_then_online() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    FakeBackend b;
    b.offline = 2;
    ach::Delivery del(reg, tr, b);

    tr.add_stat(ach::hash_key("stat_kills"), 1);
    del.pump();
    del.pump();
    check(del.stats().sent == 0, "nothing sent while offline");
    check(!del.stats().connected, "not connected while offline");
    tr.unlock(ach::hash_key("BOSS_DOWN"));
    del.pump();
    check(del.stats().sent == 3, "backlog flushed on connect");
    check(b.unlocks.size() == 2 && b.stats.size() == 1, "offline progress not lost");
    check(b.declared.size() == 3, "catalogue declared once on connect");
}

void test_retry_keeps_order() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    FakeBackend b;
    b.retry = 2;
    ach::Delivery del(reg, tr, b);

    tr.add_stat(ach::hash_key("stat_kills"), 10);
    del.pump();
    check(del.stats().sent == 0 && del.pending() == 3, "retry holds the queue head");
    del.pump();
    check(del.stats().sent == 0 && del.stats().retried == 2, "second retry consumed");
    del.pump();
    check(del.stats().sent == 3 && del.pending() == 0, "queue flushed after retries");
    check(b.unlocks.size() == 2 && b.unlocks[0] != b.unlocks[1], "retries neither lose nor duplicate");
    check(b.stats.size() == 1 && b.stats[0].second == 10, "stat survived the retries");
}

void test_fatal_degrades_silently() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    FakeBackend b;
    b.fatal = true;
    ach::Delivery del(reg, tr, b);

    tr.add_stat(ach::hash_key("stat_kills"), 10);
    del.pump();
    check(del.stats().dead, "fatal marks the backend dead");
    check(b.ended, "backend shut down");
    check(del.pending() == 0 && del.stats().dropped == 3, "queue dropped");

    tr.unlock(ach::hash_key("BOSS_DOWN"));
    del.pump();
    del.reconcile();
    check(del.stats().sent == 0, "dead backend is never used again");
    check(tr.unlocked(ach::hash_key("BOSS_DOWN")), "local progress keeps working");
}

void test_reconcile_union() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    FakeBackend b;
    b.remote.push_back("BOSS_DOWN");
    b.remote.push_back("NOT_IN_THIS_BUILD");
    ach::Delivery del(reg, tr, b);

    tr.add_stat(ach::hash_key("stat_kills"), 1);
    del.pump();
    const uint64_t sent_before = del.stats().sent;
    del.reconcile();
    check(del.stats().reconciled == 1, "remote unlock adopted");
    check(tr.unlocked(ach::hash_key("BOSS_DOWN")), "remote unlock is local now");
    check(!tr.unlocked(ach::hash_key("NOT_IN_THIS_BUILD")), "unknown remote key ignored");

    del.pump();
    check(del.stats().sent == sent_before, "adopted unlock is not echoed back");
    del.reconcile();
    check(del.stats().reconciled == 1, "second reconcile is idempotent");
}

void test_restored_progress_is_synced() {
    ach::Registry reg;
    build(reg);
    ach::Tracker src(reg);
    src.add_stat(ach::hash_key("stat_kills"), 10);
    ach::Snapshot snap;
    src.snapshot(snap);

    ach::Tracker tr(reg);
    tr.restore(snap);
    FakeBackend b;
    ach::Delivery del(reg, tr, b);
    del.pump();
    check(b.unlocks.size() == 2, "restored unlocks pushed to a fresh backend");
    check(b.stats.size() == 1 && b.stats[0].second == 10, "restored stat pushed");
}

// Стат — последнее значение, а не событие: залипший бэкенд не должен превращать очередь
// в неограниченный хвост устаревших промежуточных значений.
void test_stat_coalesced_while_stuck() {
    ach::Registry reg;
    build(reg);
    ach::Tracker tr(reg);
    FakeBackend b;
    b.stuck = true;
    ach::Delivery del(reg, tr, b);

    for (int i = 0; i < 50; ++i) {
        tr.add_stat(ach::hash_key("stat_kills"), 1);
        del.pump();
    }
    check(del.pending() == 3, "two unlocks and exactly one stat op wait in the queue");
    check(b.stats.empty(), "nothing delivered while the backend retries");

    b.stuck = false;
    del.pump();
    check(del.pending() == 0, "queue flushed once the backend recovers");
    check(b.stats.size() == 1 && b.stats[0].second == 50, "only the latest stat value is delivered");
}

} // namespace

int main() {
    std::printf("achievements delivery\n");
    test_offline_then_online();
    test_retry_keeps_order();
    test_fatal_degrades_silently();
    test_reconcile_union();
    test_restored_progress_is_synced();
    test_stat_coalesced_while_stuck();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
