#pragma once
#include <cstdio>

namespace framework::brawl::test {

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (ok) return;
    std::printf("  FAIL: %s\n", what);
    ++fails;
}

inline int verdict(const char* name) {
    std::printf("%s: %s\n", name, fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

} // namespace framework::brawl::test
