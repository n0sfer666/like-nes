#include "registry.hpp"
#include "tracker.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr uint64_t GOLDEN = 0xe728fef199e87fc9ull;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

const ach::DefSpec SPECS[] = {
    {"FIRST_BLOOD", "First Blood", "Kill one enemy", ach::Kind::Progress, "stat_kills", 1, 0},
    {"KILLER_10", "Killer", "Kill ten enemies", ach::Kind::Progress, "stat_kills", 10, 0},
    {"SCORE_500", "High Scorer", "Reach 500 points", ach::Kind::Progress, "stat_score", 500, 0},
    {"BOSS_DOWN", "Boss Down", "Defeat the boss", ach::Kind::Boolean, nullptr, 0, 0},
    {"NO_DAMAGE", "Untouchable", "Clear without damage", ach::Kind::Boolean, nullptr, 0,
     ach::FLAG_HIDDEN},
};

void build(ach::Registry& reg, bool reverse) {
    const int n = static_cast<int>(sizeof(SPECS) / sizeof(SPECS[0]));
    for (int i = 0; i < n; ++i) {
        const ach::DefSpec& s = SPECS[reverse ? n - 1 - i : i];
        check(reg.define(s) == ach::DefineResult::Ok, "define");
    }
}

uint64_t scripted(bool reverse, std::vector<ach::Event>* out) {
    ach::Registry reg;
    build(reg, reverse);
    ach::Tracker tr(reg);

    const ach::Id kills = ach::hash_key("stat_kills");
    const ach::Id score = ach::hash_key("stat_score");

    for (uint64_t t = 0; t < 600; ++t) {
        tr.set_tick(t);
        if (t % 37 == 0) tr.add_stat(kills, 1);
        if (t % 11 == 0) tr.add_stat(score, 13);
        if (t == 400) tr.unlock(ach::hash_key("BOSS_DOWN"));
        if (t == 401) tr.unlock(ach::hash_key("BOSS_DOWN"));
        if (t == 402) tr.unlock(ach::hash_key("NO_SUCH_ACHIEVEMENT"));
        if (t == 500) tr.set_stat(kills, 3);
    }
    if (out) *out = tr.events();
    return tr.hash();
}

void test_determinism() {
    std::vector<ach::Event> a;
    std::vector<ach::Event> b;
    const uint64_t ha = scripted(false, &a);
    const uint64_t hb = scripted(false, &b);
    std::printf("  hash=0x%016llx events=%zu\n",
                static_cast<unsigned long long>(ha), a.size());
    check(ha == hb, "run-to-run hash");
    check(a.size() == b.size(), "run-to-run event count");
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        check(a[i].id == b[i].id && a[i].tick == b[i].tick, "run-to-run event order");
    }
    if constexpr (GOLDEN != 0) check(ha == GOLDEN, "golden hash");

    std::vector<ach::Event> c;
    check(scripted(true, &c) == ha, "insertion-order independence");
    check(c.size() == a.size(), "insertion-order event count");
}

void test_thresholds() {
    std::vector<ach::Event> ev;
    scripted(false, &ev);
    check(ev.size() == 4, "four unlocked exactly once");
    if (ev.size() != 4) return;
    check(ev[0].id == ach::hash_key("FIRST_BLOOD") && ev[0].tick == 0, "first blood at tick 0");
    check(ev[1].id == ach::hash_key("KILLER_10") && ev[1].tick == 333, "tenth kill at tick 333");
    check(ev[2].id == ach::hash_key("BOSS_DOWN") && ev[2].tick == 400, "boss at tick 400");
    check(ev[3].id == ach::hash_key("SCORE_500") && ev[3].tick == 418, "500 points at tick 418");
}

void test_idempotence() {
    ach::Registry reg;
    build(reg, false);
    ach::Tracker tr(reg);
    const ach::Id boss = ach::hash_key("BOSS_DOWN");

    tr.unlock(boss);
    const uint64_t after_first = tr.hash();
    check(tr.events().size() == 1, "one event");
    tr.set_tick(99);
    tr.unlock(boss);
    check(tr.events().size() == 1, "repeat unlock emits nothing");
    check(tr.hash() == after_first, "repeat unlock does not move hash");

    tr.drain(1);
    check(tr.events().empty(), "drained");
    check(tr.unlocked(boss), "still unlocked after drain");
    tr.unlock(boss);
    check(tr.events().empty(), "no re-emit after drain");
}

void test_stat_semantics() {
    ach::Registry reg;
    build(reg, false);
    ach::Tracker tr(reg);
    const ach::Id kills = ach::hash_key("stat_kills");

    tr.add_stat(kills, 12);
    check(tr.stat(kills) == 12, "stat value");
    check(tr.unlocked(ach::hash_key("KILLER_10")), "threshold crossed by one jump");
    tr.set_stat(kills, 0);
    check(tr.stat(kills) == 0, "stat may decrease");
    check(tr.unlocked(ach::hash_key("KILLER_10")), "unlock never reverts");

    tr.set_stat(kills, ~0ull);
    tr.add_stat(kills, 5);
    check(tr.stat(kills) == ~0ull, "add_stat saturates");
    tr.add_stat(ach::hash_key("stat_unknown"), 1);
    check(tr.stat(ach::hash_key("stat_unknown")) == 0, "unknown stat ignored");
}

void test_define_rules() {
    ach::Registry reg;
    build(reg, false);
    check(reg.define(SPECS[0]) == ach::DefineResult::Duplicate, "duplicate id rejected");
    check(reg.entries().size() == 5, "duplicate did not land");
    check(reg.stats().size() == 2, "stats deduplicated");

    const ach::DefSpec no_target{"X1", "", "", ach::Kind::Progress, "stat_kills", 0, 0};
    const ach::DefSpec no_stat{"X2", "", "", ach::Kind::Progress, nullptr, 5, 0};
    const ach::DefSpec bool_stat{"X3", "", "", ach::Kind::Boolean, "stat_kills", 0, 0};
    const ach::DefSpec no_key{nullptr, "", "", ach::Kind::Boolean, nullptr, 0, 0};
    check(reg.define(no_target) == ach::DefineResult::BadSpec, "progress needs target");
    check(reg.define(no_stat) == ach::DefineResult::BadSpec, "progress needs stat");
    check(reg.define(bool_stat) == ach::DefineResult::BadSpec, "boolean must not bind stat");
    check(reg.define(no_key) == ach::DefineResult::BadSpec, "key required");
    check(reg.entries().size() == 5, "rejected specs did not land");

    const ach::Entry* e = reg.find(ach::hash_key("SCORE_500"));
    check(e != nullptr, "find by id");
    if (e) {
        check(e->def.target == 500 && e->def.stat == ach::hash_key("stat_score"), "def fields");
        check(std::string(e->key) == "SCORE_500", "key retained");
        check(std::string(e->name) == "High Scorer", "name retained");
    }
    check(reg.find(ach::hash_key("MISSING")) == nullptr, "missing id");
}

} // namespace

int main() {
    std::printf("achievements core\n");
    test_determinism();
    test_thresholds();
    test_idempotence();
    test_stat_semantics();
    test_define_rules();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
