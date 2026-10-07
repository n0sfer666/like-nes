#include "rumble_keys.hpp"

#include <GLFW/glfw3.h>

namespace rumble {

namespace {

bool down(GLFWwindow* window, int key, int alt) {
    return glfwGetKey(window, key) == GLFW_PRESS || glfwGetKey(window, alt) == GLFW_PRESS;
}

fix32 axis(bool minus, bool plus) { return fix32::from_int((plus ? 1 : 0) - (minus ? 1 : 0)); }

} // namespace

bool key_toggled(GLFWwindow* window, int key, bool& held) {
    const bool now = glfwGetKey(window, key) == GLFW_PRESS;
    const bool pressed = now && !held;
    held = now;
    return pressed;
}

PlayerCommand read_keys(GLFWwindow* window, KeyLatch& latch) {
    PlayerCommand c;
    framework::brawl::BrawlInput& in = c.input;
    in.present = true;
    in.move.move_x = axis(down(window, GLFW_KEY_LEFT, GLFW_KEY_A), down(window, GLFW_KEY_RIGHT, GLFW_KEY_D));
    in.move.move_z = axis(down(window, GLFW_KEY_UP, GLFW_KEY_W), down(window, GLFW_KEY_DOWN, GLFW_KEY_S));
    const bool jump = down(window, GLFW_KEY_SPACE, GLFW_KEY_K);
    if (jump && !latch.jump) in.buttons = framework::brawl::button::JUMP;
    latch.jump = jump;
    const bool punch = key_toggled(window, GLFW_KEY_J, latch.punch);
    const bool cross = key_toggled(window, GLFW_KEY_U, latch.cross);
    const bool kick = key_toggled(window, GLFW_KEY_L, latch.kick);
    if (punch) c.attack = Attack::Punch;
    else if (cross) c.attack = Attack::Cross;
    else if (kick) c.attack = Attack::Kick;
    return c;
}

} // namespace rumble
