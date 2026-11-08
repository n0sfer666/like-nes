#include "rumble_net_window.hpp"

#include <GLFW/glfw3.h>

namespace rumble {
namespace {

thread_local NetworkMode* text_target = nullptr;
void character(GLFWwindow*, unsigned int code) { if (text_target) text_target->type(code); }

} // namespace

WindowNetwork::WindowNetwork(GLFWwindow* window, NetworkMode* mode) : window_(window), mode_(mode) {
    if (mode_) {
        text_target = mode_;
        glfwSetCharCallback(window_, character);
    }
}

WindowNetwork::~WindowNetwork() {
    if (mode_) {
        glfwSetCharCallback(window_, nullptr);
        text_target = nullptr;
    }
}

void WindowNetwork::events(const WindowKeys& keys) {
    if (!mode_) return;
    if (keys.hit(WindowKeys::UP)) mode_->move(-1);
    if (keys.hit(WindowKeys::DOWN)) mode_->move(1);
    if (keys.hit(WindowKeys::ENTER)) mode_->enter();
    if (keys.hit(WindowKeys::ESCAPE)) mode_->back();
    if (keys.hit(WindowKeys::OVERLAY)) mode_->diagnostic();
    if (keys.hit(WindowKeys::ERASE)) mode_->type(8);
    mode_->poll();
}

LayerStats WindowNetwork::step(Play* play, Hotseat& hotseat, input::GamepadSource& pads, Screen& screen,
                               const Credits& credits, const framework::graphics::ViewportFit& fit, uint32_t tick) {
    pads.poll(hotseat.engine());
    mode_->advance(hotseat);
    if (play) return screen.level(*play, hotseat, credits, fit, tick, true);
    LayerStats st;
    st.scale = fit.scale;
    return st;
}

void WindowNetwork::draw(const Credits& credits, const Play* play, uint32_t w, uint32_t h,
                         Screen& screen, LayerStats& st) {
    if (mode_) quads_.add(*mode_, credits, play, w, h, st.scale, screen.layers(), st);
}

} // namespace rumble
