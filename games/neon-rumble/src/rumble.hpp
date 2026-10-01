#pragma once
#include <cstdint>

#include "schedule.hpp"

namespace rumble {

// Пустая сцена В1: одна система Sim считает тики. Счётчик сверяется с числом прогнанных кадров —
// так запуск доказывает, что расписание фреймворка из префикса реально исполняется.
struct Scene {
    framework::Schedule schedule;
    uint32_t ticks = 0;

    bool init();
    void step(uint32_t index);
};

int run_window(Scene& scene, int frames);

} // namespace rumble
