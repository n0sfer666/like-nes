#pragma once
#include "action_layout.hpp"
#include "device_state.hpp"

namespace framework::input {

// «Половина клавиатуры» игрока — клавиши его раскладки. Оси считаются обеими сторонами: стрелки
// игрока 2 — ось, и вход стрелкой — его основной путь с клавиатуры.
bool layout_key_pressed(const ::input::ActionLayout& layout, const ::input::DeviceState& now,
                        const ::input::DeviceState& prev);

} // namespace framework::input
