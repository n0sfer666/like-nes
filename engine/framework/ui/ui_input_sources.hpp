#pragma once
#include "device_state.hpp"
#include "action_layout.hpp"

namespace framework::ui::detail {
bool ui_down(const ::input::Source& source, const ::input::DeviceState& devices);
fix32 ui_axis(const ::input::Source& source, const ::input::DeviceState& devices, int pad);
}
