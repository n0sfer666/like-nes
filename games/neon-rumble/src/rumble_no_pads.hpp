#pragma once
#include "source.hpp"

namespace rumble {

class NoPads final : public input::GamepadSource {
public:
    bool init() override { return true; }
    void poll(input::InputEngine&) override {}
    void set_rumble(int, float, float, int) override {}
    const char* backend_name() const override { return "none (scripted run)"; }
};

} // namespace rumble
