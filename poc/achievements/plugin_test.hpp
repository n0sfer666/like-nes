#pragma once

namespace ach {
class Registry;
}

namespace ach_test {

extern int failures;

void check(bool ok, const char* what);
void build(ach::Registry& reg);
void test_host_lifecycle(const char* plugin_path, bool with_reload);

} // namespace ach_test
