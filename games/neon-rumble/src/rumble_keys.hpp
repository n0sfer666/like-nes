#pragma once
#include "brawl_input.hpp"

struct GLFWwindow;

namespace rumble {

bool key_toggled(GLFWwindow* window, int key, bool& held);
framework::brawl::BrawlInput read_keys(GLFWwindow* window, bool& jump_held);

} // namespace rumble
