#include "rumble_mobile.hpp"

#include <cmath>
#include <cstdio>

#include "mobile_host.hpp"

namespace rumble {
namespace {

fix32 point(float value) { return std::isfinite(value) ? fix32::from_float(value) : fix32{}; }

framework::touch::Phase phase_of(mobile::Touch phase) {
    switch (phase) {
        case mobile::Touch::Down: return framework::touch::Phase::Down;
        case mobile::Touch::Move: return framework::touch::Phase::Move;
        case mobile::Touch::Up: return framework::touch::Phase::Up;
    }
    return framework::touch::Phase::Up;
}

} // namespace

bool MobileRumble::open_controls() {
    demo_ = mobile::has_arg("--demo");
    std::string why;
    if (!hotseat_.open(!demo_) || !touch_.load(bundle_, why)) {
        std::fprintf(stderr, "neon-rumble: mobile controls: %s\n", why.c_str());
        return false;
    }
    touch_.source().bind(&hotseat_.engine());
    std::fprintf(stderr, "[game] controls %s\n", demo_ ? "demo" : "touch");
    return true;
}

void MobileRumble::fit_touch(uint32_t w, uint32_t h, const mobile::SafeArea& safe) {
    touch_scale_ = std::isfinite(safe.scale) && safe.scale > 0 ? safe.scale : 1;
    touch_.source().set_view(point(static_cast<float>(w) / touch_scale_), point(static_cast<float>(h) / touch_scale_),
        {point(safe.left), point(safe.top), point(safe.right), point(safe.bottom)});
}

void MobileRumble::pointer(intptr_t id, mobile::Touch phase, float x, float y) {
    if (!inited_ || demo_ || flow_.play() == nullptr) return;
    if (!std::isfinite(x) || !std::isfinite(y)) { cancel(); return; }
    if (phase == mobile::Touch::Down) touch_.reveal();
    touch_.source().pointer(id, phase_of(phase), point(x), point(y));
}

void MobileRumble::cancel() { touch_.source().cancel(); }
void MobileRumble::suspend() { cancel(); lag_ = 0; }
std::string MobileRumble::touch_state() const { return touch_.state(); }

} // namespace rumble
