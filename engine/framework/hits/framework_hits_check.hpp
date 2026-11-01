#pragma once
#include <cstdio>

namespace framework::hits::test {

inline int failures = 0;

inline void check(bool ok, const char* why) {
    if (ok) return;
    ++failures;
    std::printf("  FAIL: %s\n", why);
}

inline int verdict(const char* name) {
    std::printf("%s: %s\n", name, failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

} // namespace framework::hits::test
