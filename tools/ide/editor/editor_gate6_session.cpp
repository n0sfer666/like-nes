#include "editor_gate6_session.hpp"
#include "platform_env.hpp"
#include <GLFW/glfw3.h>
#include <cstdio>

namespace ide::editor {
namespace {

const char* platform_name(int p) {
    switch (p) {
    case GLFW_PLATFORM_X11:     return "x11";
    case GLFW_PLATFORM_WAYLAND: return "wayland";
    case GLFW_PLATFORM_COCOA:   return "cocoa";
    case GLFW_PLATFORM_WIN32:   return "win32";
    case GLFW_PLATFORM_NULL:    return "null";
    default:                    return "unknown";
    }
}

std::string env_or_dash(const char* name) {
    std::string v;
    return platform::env_var(name, v) ? v : std::string("-");
}

} // namespace

SessionPassport probe_session() {
    SessionPassport s;
    const int p = glfwGetPlatform();
    s.glfw_platform = platform_name(p);
    if (!platform::env_var("XDG_SESSION_TYPE", s.session_type)) s.session_type = "-";
    s.display = "DISPLAY=" + env_or_dash("DISPLAY") + " WAYLAND_DISPLAY=" + env_or_dash("WAYLAND_DISPLAY");
    s.xwayland = (s.session_type == "wayland" && p == GLFW_PLATFORM_X11);
    return s;
}

bool report_session(const SessionPassport& s) {
    std::printf("  session   : XDG_SESSION_TYPE=%s\n", s.session_type.c_str());
    std::printf("  glfw      : %s\n", s.glfw_platform.c_str());
    std::printf("  display   : %s\n", s.display.c_str());
    if (!s.xwayland) return true;

    // Ровно та подмена, ради которой гейт требует ДВЕ сессии. GLFW по умолчанию собран под X11,
    // и под Wayland-сессией бинарь живёт клиентом XWayland: окно открывается, кадры идут, всё
    // «работает» — и про Wayland не сказано ничего. Отличить это глазами нельзя, поэтому гейт
    // валится здесь, а не печатает зелёную строку про непроверенный протокол.
    std::printf("\n  FAIL: Wayland session, but the window is driven by X11 - that is XWayland, not Wayland.\n"
                "        The Wayland half of the gate is NOT checked. A separate binary is needed:\n"
                "        cmake -S . -B build-way -G Ninja -DCMAKE_BUILD_TYPE=Release -DLINUX_WAYLAND=ON\n"
                "        cmake --build build-way --target editor_shell\n");
    return false;
}

} // namespace ide::editor
