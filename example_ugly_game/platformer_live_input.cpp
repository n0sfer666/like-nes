#include "platformer_live_input.hpp"

#include <cstdio>

namespace platformer {

bool LiveInput::open(GLFWwindow* win, const std::string& bundle) {
    if (!game::load_controls(controls_, bundle) ||
        !resolve_binding(controls_.table, "default", bind_)) {
        std::fprintf(stderr, "[platformer] controls unavailable: need actions 'jump' and axes "
                             "'move_x'/'move_y' in the bundle preset\n");
        return false;
    }
    // Движок ввода строится ПОСЛЕ раскладки: он держит ссылку на её `ActionMap`, и собранный
    // раньше смотрел бы в карту, которой ещё нет.
    engine_ = std::make_unique<::input::InputEngine>(controls_.map);
    ::input::install_glfw_input(win, *engine_);
    pad_ = ::input::make_gamepad_source();
    have_pad_ = pad_ != nullptr && pad_->init();
    return true;
}

ch::MoveInput LiveInput::read(uint32_t t) {
    if (have_pad_) pad_->poll(*engine_);
    return read_input(engine_->begin_tick(t, 0), bind_);
}

const char* LiveInput::pad_name() const { return have_pad_ ? pad_->backend_name() : "none"; }

} // namespace platformer
