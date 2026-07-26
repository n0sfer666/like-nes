#include "plugin_test.hpp"

#include <cstdio>

#include "registry.hpp"

namespace ach_test {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

void build(ach::Registry& reg) {
    reg.define({"FIRST_BLOOD", "First Blood", "", ach::Kind::Progress, "stat_kills", 1, 0});
    reg.define({"KILLER_10", "Killer", "", ach::Kind::Progress, "stat_kills", 10, 0});
    reg.define({"BOSS_DOWN", "Boss Down", "", ach::Kind::Boolean, nullptr, 0, 0});
}

} // namespace ach_test
