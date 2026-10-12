#pragma once
#include <cstdint>
#include <string>

#include "scene_stack.hpp"
#include "schedule.hpp"

namespace rumble {

enum class MenuItem : uint8_t { Level, Wave, Credits };
constexpr uint32_t MENU_ITEMS = 3;

// Меню вне сессии отката: пункты level / wave / credits по уровню `level`, выбор ↑↓ и Enter — клавиши
// окна, не ввод драки. `error` — отказ последнего входа, строка над пунктами. Расписание только
// считает тики: стек сцен принимает лишь сцену с собранным расписанием.
struct Menu {
    framework::Schedule schedule;
    std::string level;
    MenuItem item = MenuItem::Level;
    std::string error;
    uint32_t ticks = 0;

    bool init(const char* level_name, bool waves);
    void move(int step);
    std::string label(MenuItem i) const;
    framework::scene::Scene stack_scene() { return {&schedule, this, false}; }
};

} // namespace rumble
