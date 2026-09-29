#include "platform_fs.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

const char* const SRC = "platform_copy_probe.src";
const char* const DST = "platform_copy_probe.dst";
const char* const MISSING = "platform_copy_probe.missing";
const char* const FRESH = "platform_copy_probe.fresh";

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "[platform-copy] FAIL: %s\n", what);
        ++fails;
    }
}

bool put(const char* path, const char* text) {
    std::FILE* f = platform::open_file(path, "wb");
    if (!f) return false;
    const size_t n = std::strlen(text);
    const bool ok = std::fwrite(text, 1, n, f) == n;
    return std::fclose(f) == 0 && ok;
}

bool holds(const char* path, const char* text) {
    std::vector<uint8_t> got;
    if (!platform::read_bytes(path, got)) return false;
    return std::string(got.begin(), got.end()) == text;
}

void clean() {
    platform::remove_file(SRC);
    platform::remove_file(DST);
    platform::remove_file(MISSING);
    platform::remove_file(FRESH);
}

} // namespace

int main() {
    clean();
    check(put(SRC, "fresh"), "writing the source");

    check(platform::copy_file_new(SRC, DST), "copy_file_new creates an absent dst");
    check(holds(DST, "fresh"), "copy_file_new copies the bytes");

    check(put(DST, "taken"), "occupying dst");
    check(!platform::copy_file_new(SRC, DST), "copy_file_new refuses an existing dst");
    check(holds(DST, "taken"), "refused copy_file_new leaves dst untouched");

    check(platform::copy_file(SRC, DST), "copy_file overwrites an existing dst");
    check(holds(DST, "fresh"), "copy_file replaces the bytes");

    check(!platform::copy_file_new(MISSING, FRESH), "copy_file_new refuses a missing source");
    check(!platform::file_exists(FRESH), "missing source leaves no orphan dst");

    clean();
    std::printf("platform-copy: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
