#pragma once
#include "rumble_command.hpp"

struct GLFWwindow;

namespace rumble {

struct KeyLatch {
    bool jump = false;
    bool punch = false;
    bool cross = false;
    bool kick = false;
    bool dodge = false;
};

bool key_toggled(GLFWwindow* window, int key, bool& held);
PlayerCommand read_keys(GLFWwindow* window, KeyLatch& latch);

} // namespace rumble
