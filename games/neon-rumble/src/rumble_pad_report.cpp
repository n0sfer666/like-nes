#include "rumble_pad_report.hpp"

#include <cstdio>

#include "source.hpp"

namespace rumble {

void report_pad_backend() {
    std::printf("neon-rumble: pad backend %s\n", ::input::make_gamepad_source()->backend_name());
}

} // namespace rumble
