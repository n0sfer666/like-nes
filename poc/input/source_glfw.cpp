#include "source.hpp"
#include <GLFW/glfw3.h>

// Пумп kbd/mouse/трекпада через OS-события GLFW-окна. GLFW-коды клавиш/кнопок совпадают с
// нашими (ASCII/GLFW), поэтому проходят как есть. Курсор → дельты (трекпад приходит как мышь).
namespace input {

namespace {
struct GlfwState {
    InputEngine* engine = nullptr;
    double last_x = 0, last_y = 0;
    double res_x = 0, res_y = 0; // дробный остаток (трекпад репортит субпиксельные дельты)
    bool has_last = false;
    uint64_t seq = 0;
    uint64_t mouse_events = 0;   // диагностика: сколько раз сработал cursor-callback с движением
};

GlfwState* state_of(GLFWwindow* w) { return static_cast<GlfwState*>(glfwGetWindowUserPointer(w)); }

void on_key(GLFWwindow* w, int key, int, int action, int) {
    if (action == GLFW_REPEAT) return; // повтор ОС фильтруем: edge только по факту up→down
    GlfwState* s = state_of(w);
    if (!s || key < 0) return;
    RawEvent e{action == GLFW_PRESS ? RawKind::KeyDown : RawKind::KeyUp,
               DeviceKind::Keyboard, 0, static_cast<uint16_t>(key), 0, s->seq++};
    s->engine->post(e);
}

void on_mouse_btn(GLFWwindow* w, int button, int action, int) {
    GlfwState* s = state_of(w);
    if (!s || button < 0) return;
    RawEvent e{action == GLFW_PRESS ? RawKind::MouseButtonDown : RawKind::MouseButtonUp,
               DeviceKind::Mouse, 0, static_cast<uint16_t>(button), 0, s->seq++};
    s->engine->post(e);
}

void on_cursor(GLFWwindow* w, double x, double y) {
    GlfwState* s = state_of(w);
    if (!s) return;
    if (s->has_last) {
        s->res_x += x - s->last_x; s->res_y += y - s->last_y; // копим субпиксельные дельты
        int dx = static_cast<int>(s->res_x), dy = static_cast<int>(s->res_y);
        s->res_x -= dx; s->res_y -= dy;                       // остаток переносим в следующий callback
        if (dx || dy) ++s->mouse_events;
        if (dx) s->engine->post({RawKind::MouseMove, DeviceKind::Mouse, 0, 0, dx, s->seq++});
        if (dy) s->engine->post({RawKind::MouseMove, DeviceKind::Mouse, 0, 1, dy, s->seq++});
    }
    s->last_x = x; s->last_y = y; s->has_last = true;
}

void on_scroll(GLFWwindow* w, double, double yoff) {
    GlfwState* s = state_of(w);
    if (!s) return;
    int wv = static_cast<int>(yoff);
    if (wv) s->engine->post({RawKind::MouseWheel, DeviceKind::Mouse, 0, 0, wv, s->seq++});
}

void on_focus(GLFWwindow* w, int focused) {
    GlfwState* s = state_of(w);
    if (!s || focused) return; // потеря фокуса → force-release всех held
    s->engine->post({RawKind::FocusLost, DeviceKind::None, 0, 0, 0, s->seq++});
}

GlfwState g_state; // одно окно демо
} // namespace

uint64_t glfw_mouse_event_count() { return g_state.mouse_events; }

void install_glfw_input(GLFWwindow* win, InputEngine& engine) {
    g_state.engine = &engine;
    glfwSetWindowUserPointer(win, &g_state);
    glfwSetKeyCallback(win, on_key);
    glfwSetMouseButtonCallback(win, on_mouse_btn);
    glfwSetCursorPosCallback(win, on_cursor);
    glfwSetScrollCallback(win, on_scroll);
    glfwSetWindowFocusCallback(win, on_focus);
}

} // namespace input
