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

int test_kill_during_write() {
#ifdef _WIN32
    std::printf("  skip kill-during-write (POSIX only)\n");
    return 0;
#else
    std::remove(SAVE_PATH);
    ach::Snapshot snap;
    for (uint64_t i = 0; i < 20000; ++i) snap.stats.push_back(ach::StatRecord{i + 1, i});
    std::vector<uint8_t> bytes;
    ach::encode(snap, bytes);
    check(ach::write_atomic(SAVE_PATH, bytes.data(), bytes.size()), "seed snapshot");

    int survived = 0;
    int mid_write = 0;
    uint64_t last_value = snap.stats[0].value;
    for (int round = 0; round < 12; ++round) {
        const pid_t pid = fork();
        if (pid == 0) {
            for (;;) {
                snap.stats[0].value += 1;
                std::vector<uint8_t> b;
                ach::encode(snap, b);
                ach::write_atomic(SAVE_PATH, b.data(), b.size());
            }
            _exit(0);
        }
        check(pid > 0, "fork");
        if (pid <= 0) return 1;
        usleep(200 + round * 350);
        kill(pid, SIGKILL);
        int status = 0;
        waitpid(pid, &status, 0);
        check(WIFSIGNALED(status) != 0, "child killed mid-write");

        const std::string tmp = std::string(SAVE_PATH) + ".tmp";
        std::FILE* leftover = std::fopen(tmp.c_str(), "rb");
        if (leftover != nullptr) {
            ++mid_write;
            std::fclose(leftover);
            std::remove(tmp.c_str());
        }

        std::vector<uint8_t> got;
        check(ach::read_file(SAVE_PATH, got), "file readable after kill");
        ach::Snapshot back;
        const ach::DecodeResult r = ach::decode(got.data(), got.size(), back);
        if (r != ach::DecodeResult::Ok) {
            ++failures;
            std::printf("  FAIL round %d: %s\n", round, ach::decode_reason(r));
        } else {
            ++survived;
            check(back.stats.size() == snap.stats.size(), "record count survived");
            check(!back.stats.empty() && back.stats[0].value >= last_value, "progress never rolls back");
            if (!back.stats.empty()) last_value = back.stats[0].value;
        }
    }
    std::printf("  kill-during-write: %d/12 clean, %d killed between temp and rename\n",
                survived, mid_write);
    check(mid_write > 0, "kill actually landed mid-write at least once");
    return 0;
#endif
}

} // namespace

int main() {
    std::printf("achievements crash-during-write\n");
    test_kill_during_write();
    std::remove(SAVE_PATH);
    std::remove((std::string(SAVE_PATH) + ".tmp").c_str());
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
