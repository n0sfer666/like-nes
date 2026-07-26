#include "state.hpp"
#include "store.hpp"

#include <cstdio>
#include <string>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

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
    std::remove(SAVE_PATH);
    std::remove(ach::temp_path_for(SAVE_PATH).c_str());
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

    std::FILE* stale = std::fopen(ach::temp_path_for(SAVE_PATH).c_str(), "wb");
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

    std::FILE* leftover = std::fopen(ach::temp_path_for(SAVE_PATH).c_str(), "rb");
    check(leftover == nullptr, "temp gone after successful write");
    if (leftover != nullptr) std::fclose(leftover);
}

void test_kill_during_write() {
#ifdef _WIN32
    std::printf("  skip kill-during-write (POSIX only)\n");
#else
    std::remove(SAVE_PATH);
    std::remove(ach::temp_path_for(SAVE_PATH).c_str());
    ach::Snapshot even;
    ach::Snapshot odd;
    fill(even, 0xa5a5a5a5a5a5a5a5ull);
    fill(odd, 0x5a5a5a5a5a5a5a5aull);
    std::vector<uint8_t> bytes_even;
    std::vector<uint8_t> bytes_odd;
    ach::encode(even, bytes_even);
    ach::encode(odd, bytes_odd);
    check(bytes_even.size() == bytes_odd.size(), "images are the same size");
    check(bytes_even != bytes_odd, "images differ");
    check(ach::write_atomic(SAVE_PATH, bytes_even.data(), bytes_even.size()), "seed snapshot");

    int intact = 0;
    int mid_write = 0;
    int replaced = 0;
    for (int round = 0; round < 12; ++round) {
        const pid_t pid = fork();
        if (pid == 0) {
            for (;;) {
                ach::write_atomic(SAVE_PATH, bytes_odd.data(), bytes_odd.size());
                ach::write_atomic(SAVE_PATH, bytes_even.data(), bytes_even.size());
            }
            _exit(0);
        }
        check(pid > 0, "fork");
        if (pid <= 0) return;
        usleep(200 + round * 350);
        kill(pid, SIGKILL);
        int status = 0;
        waitpid(pid, &status, 0);
        check(WIFSIGNALED(status) != 0 && WTERMSIG(status) == SIGKILL, "child killed mid-write");

        const std::string tmp = ach::temp_path_for(SAVE_PATH);
        std::FILE* leftover = std::fopen(tmp.c_str(), "rb");
        if (leftover != nullptr) {
            ++mid_write;
            std::fclose(leftover);
            std::remove(tmp.c_str());
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
#endif
}

} // namespace

int main() {
    std::printf("achievements crash-during-write\n");
    test_overwrite_is_complete();
    test_kill_during_write();
    std::remove(SAVE_PATH);
    std::remove(ach::temp_path_for(SAVE_PATH).c_str());
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
