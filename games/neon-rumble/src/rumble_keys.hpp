#pragma once
#include <array>
#include <cstdint>

struct GLFWwindow;

namespace rumble {

// Клавиши окна опрашиваются все в каждом кадре, какая бы сцена ни стояла: `held` клавиши, которую
// сцена не читает, иначе устарел бы, и нажатие после смены сцены сработало бы без нажатия. Нажатие
// копится до `consume`: кадр без текстуры его не теряет.
class WindowKeys {
public:
    enum Key : uint8_t { CREDITS, OVERLAY, UP, DOWN, ENTER, ESCAPE, COUNT };

    void poll(GLFWwindow* window);
    void press_code(uint16_t glfw_key);
    bool hit(Key k) const { return pressed_[k]; }
    void consume() { pressed_.fill(false); }

private:
    std::array<bool, COUNT> held_{};
    std::array<bool, COUNT> pressed_{};
};

} // namespace rumble
