#pragma once
#include <string>
namespace game::test {
void fail_next_write_to(const std::string& path, bool after_replace);
int injected_failures();
}
