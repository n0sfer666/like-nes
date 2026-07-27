#include "state.hpp"
#include "store.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

const char* const SAVE_PATH = "ach_crash_test.save";

void fill(ach::Snapshot& snap, uint64_t generation) {
    snap.stats.clear();
    for (uint64_t i = 0; i < 20000; ++i) snap.stats.push_back(ach::StatRecord{i + 1, generation});
}

void test_overwrite_is_complete() {
    platform::remove_file(SAVE_PATH);
    platform::remove_file(ach::temp_path_for(SAVE_PATH));
    ach::Snapshot big;
    ach::Snapshot small;
    fill(big, 0xa5a5a5a5a5a5a5a5ull);
    small.stats.push_back(ach::StatRecord{1, 7});
    std::vector<uint8_t> bytes_big;
    std::vector<uint8_t> bytes_small;
    ach::encode(big, bytes_big);
    ach::encode(small, bytes_small);
    check(bytes_small.size() < bytes_big.size(), "short image is shorter than long one");

    std::vector<uint8_t> got;
    check(ach::write_atomic(SAVE_PATH, bytes_big.data(), bytes_big.size()), "write long image");
    check(ach::read_file(SAVE_PATH, got) && got == bytes_big, "long image landed");

    std::FILE* stale = platform::open_file(ach::temp_path_for(SAVE_PATH), "wb");
    check(stale != nullptr, "seed stale temp");
    if (stale != nullptr) {
        check(std::fwrite(bytes_big.data(), 1, bytes_big.size(), stale) == bytes_big.size(),
              "stale temp fully written");
        check(std::fclose(stale) == 0, "stale temp closed");
    }
    check(ach::write_atomic(SAVE_PATH, bytes_small.data(), bytes_small.size()),
          "write short image over long one");
    check(ach::read_file(SAVE_PATH, got) && got == bytes_small,
          "short image fully replaced the long one");

    check(!platform::file_exists(ach::temp_path_for(SAVE_PATH)), "temp gone after successful write");
}

// Два образа одинаковой длины и разного содержимого: половинчатая запись видна как «ни тот,
// ни другой», а не как «стало короче». Обязаны совпадать у родителя и у ребёнка.
void make_images(std::vector<uint8_t>& bytes_even, std::vector<uint8_t>& bytes_odd) {
    ach::Snapshot even;
    ach::Snapshot odd;
    fill(even, 0xa5a5a5a5a5a5a5a5ull);
    fill(odd, 0x5a5a5a5a5a5a5a5aull);
    ach::encode(even, bytes_even);
    ach::encode(odd, bytes_odd);
}

std::string ready_path_for(const std::string& save_path) { return save_path + ".ready"; }

// Ребёнок: бесконечно перезаписывает сейв двумя образами. Его убьют на середине.
int run_writer(const char* save_path) {
    std::vector<uint8_t> bytes_even;
    std::vector<uint8_t> bytes_odd;
    make_images(bytes_even, bytes_odd);
    // Флаг «вошёл в цикл» — точка отсчёта для родителя. Без него пауза мерилась бы от спавна, а
    // он стоит десятки миллисекунд (антивирусный фильтр на Windows), и весь разброс раундов
    // уходил бы в старт процесса вместо фаз транзакции.
    std::FILE* ready = platform::open_file(ready_path_for(save_path), "wb");
    if (ready != nullptr) std::fclose(ready);
    for (;;) {
        ach::write_atomic(save_path, bytes_odd.data(), bytes_odd.size());
        ach::write_atomic(save_path, bytes_even.data(), bytes_even.size());
    }
}

// true = ребёнок дошёл до цикла записи. Ждём активно: интерес представляют микропаузы после
// готовности, а не собственная латентность старта процесса.
bool wait_ready(const std::string& save_path) {
    const std::string flag = ready_path_for(save_path);
    for (int i = 0; i < 2000; ++i) {
        if (platform::file_exists(flag)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

void test_kill_during_write() {
    const std::string self = platform::exe_path();
    if (self.empty()) { check(false, "own exe path resolved"); return; }

    platform::remove_file(SAVE_PATH);
    platform::remove_file(ach::temp_path_for(SAVE_PATH));
    std::vector<uint8_t> bytes_even;
    std::vector<uint8_t> bytes_odd;
    make_images(bytes_even, bytes_odd);
    check(bytes_even.size() == bytes_odd.size(), "images are the same size");
    check(bytes_even != bytes_odd, "images differ");
    check(ach::write_atomic(SAVE_PATH, bytes_even.data(), bytes_even.size()), "seed snapshot");

    int intact = 0;
    int mid_write = 0;
    int replaced = 0;
    const std::string ready = ready_path_for(SAVE_PATH);
    for (int round = 0; round < 12; ++round) {
        platform::remove_file(ready);
        platform::Child writer;
        check(writer.spawn({self, "--writer", SAVE_PATH}), "spawn writer child");
        if (!writer.alive()) return;
        if (!wait_ready(SAVE_PATH)) { check(false, "writer child reached its loop"); return; }
        // Пауза растёт от раунда к раунду: убийство должно попадать в разные точки транзакции —
        // и до temp, и между temp и rename. Иначе гейт проверяет одну-единственную фазу. Отсчёт —
        // от флага готовности, поэтому шкала микросекундная: старт процесса из неё исключён.
        std::this_thread::sleep_for(std::chrono::microseconds(200 + round * 350));
        check(writer.kill_and_wait(), "child killed mid-write");

        const std::string tmp = ach::temp_path_for(SAVE_PATH);
        if (platform::file_exists(tmp)) {
            ++mid_write;
            platform::remove_file(tmp);
        }

        std::vector<uint8_t> got;
        if (!ach::read_file(SAVE_PATH, got)) {
            check(false, "target readable after kill");
            continue;
        }
        if (got == bytes_even || got == bytes_odd) {
            ++intact;
            if (got == bytes_odd) ++replaced;
        } else {
            ++failures;
            ach::Snapshot back;
            const ach::DecodeResult r = ach::decode(got.data(), got.size(), back);
            std::printf("  FAIL round %d: target is neither image (%zu of %zu bytes, decode: %s)\n",
                        round, got.size(), bytes_even.size(), ach::decode_reason(r));
        }
    }
    std::printf("  kill-during-write: %d/12 intact, %d replaced, %d killed between temp and rename\n",
                intact, replaced, mid_write);
    check(mid_write > 0, "kill actually landed mid-write at least once");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    // Служебный режим: тест перезапускает сам себя как пишущего ребёнка (fork'а на Windows нет).
    if (argc == 3 && std::string(argv[1]) == "--writer") return run_writer(argv[2]);

    std::printf("achievements crash-during-write\n");
    test_overwrite_is_complete();
    test_kill_during_write();
    platform::remove_file(SAVE_PATH);
    platform::remove_file(ach::temp_path_for(SAVE_PATH));
    platform::remove_file(ready_path_for(SAVE_PATH));
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
