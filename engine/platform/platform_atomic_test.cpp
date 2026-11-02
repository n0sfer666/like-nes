#include "platform_fs.hpp"

#include <cstring>

// Настоящая fwrite/fclose в tmpfile, мок только platform boundary. Отказы до rename
// не должны трогать цель; отказ sync каталога после rename обязан дойти до вызывающего.
namespace {
int fails, mode;
std::string calls;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("atomic-protocol: FAIL %s\n", why); } }
}
namespace platform {
std::FILE* open_file(const std::string& path, const char* flags) {
    calls += "open;";
    check(path == "slot.tmp" && std::strcmp(flags, "wb") == 0, "temp path and mode");
    return mode == 1 ? nullptr : std::tmpfile();
}
bool sync_file(std::FILE* file) {
    calls += "sync;";
    check(std::fflush(file) == 0 && std::ftell(file) == 3, "all bytes precede sync");
    std::rewind(file);
    uint8_t data[3]{};
    check(std::fread(data, 1, 3, file) == 3 && data[0] == 1 && data[1] == 0 && data[2] == 255, "exact binary payload");
    return mode != 2;
}
bool replace_file(const std::string& from, const std::string& to) {
    calls += "replace;";
    check(from == "slot.tmp" && to == "slot", "replace exact target");
    return mode != 3;
}
bool sync_dir_of(const std::string& path) {
    calls += "directory;"; check(path == "slot", "sync target directory"); return mode != 4;
}
bool remove_file(const std::string& path) {
    calls += "remove;"; check(path == "slot.tmp", "cleanup only temp"); return mode != 5;
}
}

int main() {
    const uint8_t bytes[]{1, 0, 255};
    for (mode = 0; mode <= 5; ++mode) {
        calls.clear();
        check(platform::write_atomic("slot", bytes) == (mode == 0), "result matches failure phase");
        const char* expected[]{"remove;open;sync;replace;directory;", "remove;open;", "remove;open;sync;remove;",
                               "remove;open;sync;replace;remove;", "remove;open;sync;replace;directory;", "remove;"};
        check(calls == expected[mode], "ordered transaction and no rename before durability");
    }
    std::puts(fails ? "atomic-protocol: FAIL" : "atomic-protocol: PASS ordered write/sync/replace/directory and failures");
    return fails ? 1 : 0;
}
