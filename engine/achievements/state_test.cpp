#include "platform_fs.hpp"
#include "registry.hpp"
#include "state.hpp"
#include "store.hpp"
#include "tracker.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

const char* const SAVE_PATH = "ach_state_test.save";

void build(ach::Registry& reg) {
    check(reg.define({"FIRST_BLOOD", "First Blood", "", ach::Kind::Progress, "stat_kills", 1, 0}) ==
              ach::DefineResult::Ok, "define first blood");
    check(reg.define({"KILLER_10", "Killer", "", ach::Kind::Progress, "stat_kills", 10, 0}) ==
              ach::DefineResult::Ok, "define killer");
    check(reg.define({"BOSS_DOWN", "Boss Down", "", ach::Kind::Boolean, nullptr, 0, 0}) ==
              ach::DefineResult::Ok, "define boss");
}

void test_codec() {
    ach::Snapshot snap;
    snap.stats.push_back(ach::StatRecord{ach::hash_key("stat_kills"), 42});
    snap.stats.push_back(ach::StatRecord{ach::hash_key("stat_score"), 1234567890123ull});
    snap.unlocked.push_back(ach::hash_key("BOSS_DOWN"));

    std::vector<uint8_t> bytes;
    ach::encode(snap, bytes);
    check(bytes.size() == ach::STATE_HEADER_SIZE + 2 * ach::STATE_STAT_SIZE + ach::STATE_ACH_SIZE,
          "encoded size");

    ach::Snapshot back;
    check(ach::decode(bytes.data(), bytes.size(), back) == ach::DecodeResult::Ok, "decode ok");
    check(back.stats.size() == 2 && back.unlocked.size() == 1, "counts");
    check(back.stats[0].id == snap.stats[0].id && back.stats[0].value == 42, "stat 0");
    check(back.stats[1].value == 1234567890123ull, "stat 1");
    check(back.unlocked[0] == snap.unlocked[0], "unlocked id");

    std::vector<uint8_t> bad = bytes;
    bad[ach::STATE_HEADER_SIZE] ^= 0x01;
    check(ach::decode(bad.data(), bad.size(), back) == ach::DecodeResult::BadHash, "payload corrupt");

    bad = bytes;
    bad.pop_back();
    check(ach::decode(bad.data(), bad.size(), back) == ach::DecodeResult::BadSize, "truncated");

    bad = bytes;
    bad[4] = 99;
    check(ach::decode(bad.data(), bad.size(), back) == ach::DecodeResult::BadVersion, "version");

    bad = bytes;
    bad[0] = 'X';
    check(ach::decode(bad.data(), bad.size(), back) == ach::DecodeResult::BadMagic, "magic");

    check(ach::decode(bytes.data(), 4, back) == ach::DecodeResult::TooShort, "too short");
    check(ach::decode(nullptr, 0, back) == ach::DecodeResult::TooShort, "null");

    ach::Snapshot empty;
    ach::encode(empty, bytes);
    check(ach::decode(bytes.data(), bytes.size(), back) == ach::DecodeResult::Ok, "empty snapshot");
    check(back.stats.empty() && back.unlocked.empty(), "empty round-trip");
}

void test_roundtrip_through_tracker() {
    platform::remove_file(SAVE_PATH);
    ach::Registry reg;
    build(reg);
    const ach::Id kills = ach::hash_key("stat_kills");
    const ach::LocalStore store(SAVE_PATH);

    ach::Tracker a(reg);
    a.set_tick(7);
    a.add_stat(kills, 4);
    a.unlock(ach::hash_key("BOSS_DOWN"));
    check(store.save(a), "save");

    ach::Tracker b(reg);
    ach::DecodeResult why = ach::DecodeResult::Ok;
    check(store.load(b, &why), "load");
    check(why == ach::DecodeResult::Ok, ach::decode_reason(why));
    check(b.progress_hash() == a.progress_hash(), "progress restored");
    check(b.stat(kills) == 4, "stat restored");
    check(b.unlocked(ach::hash_key("BOSS_DOWN")), "unlock restored");
    check(b.unlocked(ach::hash_key("FIRST_BLOOD")), "threshold restored");
    check(b.events().empty(), "restore emits no event for already unlocked");

    ach::Registry shrunk;
    check(shrunk.define({"BOSS_DOWN", "", "", ach::Kind::Boolean, nullptr, 0, 0}) ==
              ach::DefineResult::Ok, "define shrunk");
    ach::Tracker c(shrunk);
    check(store.load(c), "load into shrunk registry");
    check(c.unlocked(ach::hash_key("BOSS_DOWN")), "known id survived");
    check(c.unlocked_count() == 1, "unknown ids not applied");

    // Регресс: запись снимка при урезанном каталоге (нет бандла, стаб-сборка, откат версии) не
    // должна стирать чужой прогресс — иначе один запуск без каталога обнуляет игрока.
    check(c.carried_count() == 2, "unknown records carried, not lost");
    check(store.save(c), "save from a shrunk registry");
    ach::Tracker d(reg);
    check(store.load(d), "load back into the full registry");
    check(d.progress_hash() == a.progress_hash(), "records outside the shrunk catalogue survived");
}

void test_retroactive_threshold() {
    platform::remove_file(SAVE_PATH);
    ach::Registry reg;
    build(reg);
    const ach::Id kills = ach::hash_key("stat_kills");
    const ach::LocalStore store(SAVE_PATH);

    ach::Tracker a(reg);
    a.add_stat(kills, 5);
    check(store.save(a), "save mid-progress");

    ach::Registry lowered;
    check(lowered.define({"KILLER_10", "", "", ach::Kind::Progress, "stat_kills", 3, 0}) ==
              ach::DefineResult::Ok, "define lowered");
    ach::Tracker b(lowered);
    check(store.load(b), "load with lowered target");
    check(b.unlocked(ach::hash_key("KILLER_10")), "lowered target unlocks on restore");
    check(b.events().size() == 1, "restore emits for newly crossed");
}

// Потолок переноса (аудит #21, A·2·4) — разбор у Tracker::MAX_CARRIED, здесь предмет гейта: без
// потолка сейв на 8 МиБ из ≈миллиона неизвестных каталогу записей клал ЗАГРУЗКУ на минуты-часы, не
// нарушая ни формата, ни хеша целостности.
//
// Прогон стоит квадрат по MAX_CARRIED — тот же линейный поиск дубликата, ради которого потолок и
// заведён: сегодня это ≈2×8,4 млн сравнений, миллисекунды. Поднимут константу на порядок — гейт
// сам станет тем зависанием, от которого сторожит, и упадёт по таймауту вместо красной строки.
void test_the_carry_has_a_ceiling() {
    constexpr std::size_t MAX = ach::Tracker::MAX_CARRIED;
    ach::Registry reg;
    build(reg);
    ach::Snapshot snap;
    for (std::size_t i = 0; i < MAX; ++i) {
        snap.stats.push_back(ach::StatRecord{static_cast<ach::Id>(0x1000 + i), i});
        snap.unlocked.push_back(static_cast<ach::Id>(0x900000 + i));
    }
    ach::Tracker edge(reg);
    check(edge.restore(snap) == 2 * MAX, "every record outside the catalogue is counted");
    check(edge.carried_count() == 2 * MAX, "control: a save exactly at the ceiling is carried whole");
    check(edge.dropped_count() == 0, "and nothing is dropped at the ceiling itself");

    snap.stats.push_back(ach::StatRecord{0x2000, 7});
    snap.unlocked.push_back(0x8fffff);
    // Дубликат уже перенесённого — в КАЖДОМ из двух списков, а не в одном: у stats и unlocked свои
    // векторы и свои копии проверки, и гейт на одном только stats оставил бы ровно половину
    // квадрата — миллион × MAX_CARRIED сравнений по unlocked — незамеченной.
    snap.stats.push_back(ach::StatRecord{0x1000, 999});
    snap.unlocked.push_back(0x900000);
    ach::Tracker over(reg);
    check(over.restore(snap) == 2 * MAX + 4, "records past the ceiling count as unknown too");
    check(over.carried_count() == 2 * MAX, "but the carry itself stops at the ceiling");
    check(over.dropped_count() == 4, "and the tracker says how many of them it dropped");

    // Порядок внутри переноса утверждается ПОВЕДЕНИЕМ, а не чтением кода: потолок спрашивается ДО
    // поиска дубликата, поэтому запись с уже перенесённым идентификатором за потолком отбрасывается
    // и теряет своё обновление. Спроси его ПОСЛЕ поиска — дубликат вышел бы по совпадению, счётчик
    // отброшенных не вырос бы, а у stats доехало бы ещё и новое значение. Квадрат при этом остался
    // бы на месте, и обе разницы видны только отсюда.
    ach::Snapshot out;
    over.snapshot(out);
    uint64_t first = 999;
    for (const ach::StatRecord& s : out.stats)
        if (s.id == 0x1000) first = s.value;
    check(first == 0, "the ceiling is asked BEFORE the duplicate scan, so a late duplicate is dropped");
}

void test_stale_temp() {
    platform::remove_file(SAVE_PATH);
    ach::Registry reg;
    build(reg);
    const ach::LocalStore store(SAVE_PATH);
    ach::Tracker a(reg);
    a.add_stat(ach::hash_key("stat_kills"), 11);
    check(store.save(a), "save before crash");

    std::FILE* junk = platform::open_file(store.temp_path(), "wb");
    check(junk != nullptr, "stale temp created");
    if (junk) {
        std::fwrite("GARBAGE", 1, 7, junk);
        std::fclose(junk);
    }

    ach::Tracker b(reg);
    check(store.load(b), "stale temp does not affect load");
    check(b.stat(ach::hash_key("stat_kills")) == 11, "previous snapshot intact");
    check(store.save(b), "save over stale temp");
    platform::remove_file(store.temp_path());
}

} // namespace

int main() {
    std::printf("achievements persistence\n");
    test_codec();
    test_roundtrip_through_tracker();
    test_retroactive_threshold();
    test_the_carry_has_a_ceiling();
    test_stale_temp();
    platform::remove_file(SAVE_PATH);
    platform::remove_file(std::string(SAVE_PATH) + ".tmp");
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
