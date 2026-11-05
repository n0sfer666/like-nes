#include "rumble_mobile.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

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
    last_jab_tick_ = jab_elapsed_ = 0;
    was_jab_ = false;
    demo_ = mobile::has_arg("--demo");
    std::string why;
    if (!hotseat_.open(!demo_) || !touch_.load(bundle_, why)) {
        std::fprintf(stderr, "neon-rumble: mobile controls: %s\n", why.c_str());
        return false;
    }
    touch_.source().bind(&hotseat_.engine());
    pads_ = demo_ ? nullptr : input::make_gamepad_source();
    if (pads_ && !pads_->init()) {
        std::fprintf(stderr, "[game] pad backend %s did not start\n", pads_->backend_name());
        pads_ = nullptr;
    }
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
// Диагностика вне Brawl/снапшотов переживает конец клипа: медленный эмулятор не обязан
// попасть пробой ровно в нужный кадр анимации длиной 240мс.
void MobileRumble::observe_punch(const Play& play) {
    const auto* body = play.brawl.pool.find(play.brawl.seats.at[PLAYER].body);
    const bool jab = body != nullptr && body->kind < FIGHTERS &&
        body->move != framework::brawl::NO_STRIKE &&
        std::strcmp(play.fighters[body->kind].pose(*body).name, "banderas/jab") == 0;
    if (jab && (!was_jab_ || body->elapsed < jab_elapsed_)) last_jab_tick_ = play.scene.ticks;
    was_jab_ = jab;
    jab_elapsed_ = jab ? body->elapsed : 0;
}
std::string MobileRumble::touch_state() const {
    std::string state = touch_.state();
    const Play* play = flow_.play();
    if (play == nullptr) return state;
    const auto* body = play->brawl.pool.find(play->brawl.seats.at[PLAYER].body);
    state += " tick=" + std::to_string(play->scene.ticks);
    state += " jab_tick=" + std::to_string(last_jab_tick_);
    state += demo_ ? " demo=1" : " demo=0";
    if (body == nullptr || body->kind >= FIGHTERS) return state + " player=0";
    const Pose pose = play->fighters[body->kind].pose(*body);
    return state + " player=1 player_x=" + std::to_string(body->pos.x.to_int()) +
        " player_z=" + std::to_string(body->pos.z.to_int()) + " pose=" + pose.name;
}

} // namespace rumble
