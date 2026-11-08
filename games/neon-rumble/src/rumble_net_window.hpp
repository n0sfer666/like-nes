#pragma once
#include "rumble_keys.hpp"
#include "rumble_net_quads.hpp"
#include "source.hpp"

namespace rumble {

class WindowNetwork {
public:
    WindowNetwork(GLFWwindow* window, NetworkMode* mode);
    ~WindowNetwork();
    void events(const WindowKeys& keys);
    LayerStats step(Play* play, Hotseat& hotseat, input::GamepadSource& pads, Screen& screen,
                    const Credits& credits, const framework::graphics::ViewportFit& fit, uint32_t tick);
    void draw(const Credits& credits, const Play* play, uint32_t w, uint32_t h, Screen& screen, LayerStats& st);

private:
    GLFWwindow* window_;
    NetworkMode* mode_;
    NetworkQuads quads_;
};

} // namespace rumble
