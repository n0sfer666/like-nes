#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"
#include "rumble_credits.hpp"
#include "rumble_fighter.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_level.hpp"
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
    void step(uint32_t index);
};

int run_window(Scene& scene, Hotseat& hotseat, const Level& level, const Fighters& fighters, const Credits& credits, int frames);

} // namespace rumble
