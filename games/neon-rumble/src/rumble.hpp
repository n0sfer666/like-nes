#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"
#include "schedule.hpp"

namespace rumble {

// Сцена: одна система Sim считает тики и шагает драку `brawl`, которую отдаёт `init`. Счётчик
// сверяется с числом прогнанных кадров — так запуск доказывает, что расписание фреймворка из
// префикса реально исполняется.
struct Scene {
    framework::Schedule schedule;
    Brawl* brawl = nullptr;
    uint32_t ticks = 0;

    bool init(Brawl& fight);
};

framework::Tick frame_tick(uint32_t index);

} // namespace rumble
