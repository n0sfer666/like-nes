#include "rumble_keys.hpp"

#include <GLFW/glfw3.h>

namespace rumble {

namespace {

bool key_toggled(GLFWwindow* window, int key, bool& held) {
    const bool now = glfwGetKey(window, key) == GLFW_PRESS;
    const bool pressed = now && !held;
    held = now;
    return pressed;
}

} // namespace

void WindowKeys::poll(GLFWwindow* window) {
    static constexpr std::array<int, COUNT> GLFW_KEYS = {GLFW_KEY_F1, GLFW_KEY_F3, GLFW_KEY_UP,
                                                         GLFW_KEY_DOWN, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE};
    for (uint32_t i = 0; i < COUNT; ++i) pressed_[i] = key_toggled(window, GLFW_KEYS[i], held_[i]) || pressed_[i];
}

} // namespace rumble
