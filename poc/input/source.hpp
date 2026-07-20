#pragma once
#include "engine.hpp"

// HAL-граница ввода. Геймпад — platform-native бэкенд (XInput / GameController / evdev) за
// интерфейсом GamepadSource. Kbd/mouse/трекпад — через OS-события GLFW-окна (тонкий пумп;
// raw-HID kbd/mouse per-OS = точка расширения). Оба кормят InputEngine сырыми RawEvent.
struct GLFWwindow;

namespace input {

class GamepadSource {
public:
    virtual ~GamepadSource() = default;
    virtual bool init() = 0;
    virtual void poll(InputEngine& e) = 0;                      // эмит button/axis/connect/disconnect
    virtual void set_rumble(int slot, float low, float high, int ms) = 0;
    virtual const char* backend_name() const = 0;
};

// Фабрика native-бэкенда для текущей ОС (nullptr-safe заглушка, если платформа не собрана).
GamepadSource* make_gamepad_source();

// Установить GLFW-колбэки kbd/mouse/scroll/focus → InputEngine.post (пумп окна).
void install_glfw_input(GLFWwindow* win, InputEngine& engine);

// Диагностика: число cursor-callback'ов с ненулевой пиксельной дельтой (проверить захват мыши).
uint64_t glfw_mouse_event_count();

} // namespace input
