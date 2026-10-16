#pragma once
#include <cstdint>

#include "input_engine.hpp"
#include "rumble_command.hpp"

namespace rumble {

// Скрипт хотсита: команда P1 по тику драки и клавиши P2 по её часам (первый шаг — тик 1). Один на
// headless-прогон и на демо телефона: голден headless тогда судит и тот ввод, что видит телефон.
PlayerCommand demo_p1(uint32_t t);
// P2 входит нажатием нампада на тике 2 (тик 1 только засевает лобби), сидит с тика 3 и держит блок
// 31..40 — улика, что его ввод доходит до драки.
void demo_p2(input::InputEngine& engine, uint32_t t);

} // namespace rumble
