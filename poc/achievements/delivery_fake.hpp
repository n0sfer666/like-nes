#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "backend.hpp"
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

class FakeBackend final : public ach::Backend {
public:
    int offline = 0;
    int retry = 0;
    bool stuck = false;
    bool fatal = false;
    int commits = 0;
    bool ended = false;
    std::vector<std::string> unlocks;
    std::vector<std::string> declared;
    std::vector<std::pair<std::string, uint64_t>> stats;
    std::vector<std::string> remote;
    std::vector<std::string> polled;

    bool begin() override {
        if (offline > 0) {
            --offline;
            return false;
        }
        return true;
    }
    void declare(const char* key) override { declared.push_back(key); }
    ach::Send unlock(const char* key) override {
        const ach::Send r = gate();
        if (r == ach::Send::Ok) unlocks.push_back(key);
        return r;
    }
    ach::Send set_stat(const char* key, uint64_t value) override {
        const ach::Send r = gate();
        if (r == ach::Send::Ok) stats.emplace_back(key, value);
        return r;
    }
    ach::Send commit() override {
        ++commits;
        return ach::Send::Ok;
    }
    // Ключи принадлежат бэкенду и обязаны пережить вызов: доставка читает их уже после возврата.
    int32_t poll_remote(const char** out, int32_t cap) override {
        polled.clear();
        polled.swap(remote);
        int32_t n = 0;
        for (const std::string& k : polled) {
            if (n >= cap) break;
            out[n++] = k.c_str();
        }
        return n;
    }
    void end() override { ended = true; }

private:
    ach::Send gate() {
        if (stuck) return ach::Send::Retry;
        if (fatal) {
            fatal = false;
            return ach::Send::Fatal;
        }
        if (retry > 0) {
            --retry;
            return ach::Send::Retry;
        }
        return ach::Send::Ok;
    }
};

void build(ach::Registry& reg) {
    reg.define({"FIRST_BLOOD", "First Blood", "", ach::Kind::Progress, "stat_kills", 1, 0});
    reg.define({"KILLER_10", "Killer", "", ach::Kind::Progress, "stat_kills", 10, 0});
    reg.define({"BOSS_DOWN", "Boss Down", "", ach::Kind::Boolean, nullptr, 0, 0});
}

} // namespace
