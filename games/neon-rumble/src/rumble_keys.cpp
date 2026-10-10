#include "rumble_keys.hpp"

#include <GLFW/glfw3.h>

namespace rumble {

bool key_toggled(GLFWwindow* window, int key, bool& held) {
    const bool now = glfwGetKey(window, key) == GLFW_PRESS;
    const bool pressed = now && !held;
    held = now;
    return pressed;
}

} // namespace rumble
