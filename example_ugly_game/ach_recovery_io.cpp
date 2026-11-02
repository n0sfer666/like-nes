// Test-only narrow injection around the actual platform atomic writer (renamed in an OBJECT target).
#include "ach_recovery_io.hpp"
#include "platform_fs.hpp"

namespace {
std::string target;
bool after;
int failures;
}
namespace game::test {
void fail_next_write_to(const std::string& path, bool after_replace) { target = path; after = after_replace; }
int injected_failures() { return failures; }
}
namespace platform {
bool ach_recovery_write_atomic_real(const std::string& path, std::span<const uint8_t> bytes);
bool write_atomic(const std::string& path, std::span<const uint8_t> bytes) {
    if (path != target) return ach_recovery_write_atomic_real(path, bytes);
    target.clear(); ++failures;
    if (after) (void)ach_recovery_write_atomic_real(path, bytes);
    return false;
}
}
