#include "rumble_hotseat.hpp"

#include <cstdio>
#include <string>

#include "rumble_controls.hpp"

namespace rumble {

namespace {

using framework::input::SeatState;

const char* state_name(SeatState s) {
    switch (s) {
    case SeatState::Joining: return "joining";
    case SeatState::Present: return "present";
    case SeatState::Lost: return "lost the pad, paused";
    case SeatState::Resuming: return "resuming";
    case SeatState::Free: break;
    }
    return "free";
}

} // namespace

bool Hotseat::open() {
    std::string why;
    if (!set_layouts(map_, why)) {
        std::fprintf(stderr, "neon-rumble: controls: %s\n", why.c_str());
        return false;
    }
    if (!lobby_.join(0, input::PlayerAssign{-1, true})) {
        std::fprintf(stderr, "neon-rumble: player 1 does not get the keyboard\n");
        return false;
    }
    for (uint32_t p = 0; p < was_.size(); ++p) was_[p] = lobby_.state(static_cast<int>(p));
    return true;
}

bool Hotseat::tick(uint32_t t, Players& out) {
    engine_.drain();
    lobby_.update(engine_.device());
    for (uint32_t p = 0; p < out.size(); ++p) {
        const int player = static_cast<int>(p);
        const input::InputFrame& f = engine_.resolve(t, player);
        out[p] = lobby_.present(player) ? command_of(f) : PlayerCommand{};
        if (lobby_.present(player) && f.action_pressed(LEAVE)) {
            lobby_.leave(player);
            out[p] = PlayerCommand{};
        }
    }
    report(t);
    return !lobby_.paused();
}

void Hotseat::report(uint32_t t) {
    for (uint32_t p = 0; p < was_.size(); ++p) {
        const SeatState now = lobby_.state(static_cast<int>(p));
        if (now == was_[p]) continue;
        was_[p] = now;
        std::printf("neon-rumble: seat tick %u: P%u %s\n", t, p + 1, state_name(now));
        std::fflush(stdout);
    }
}

} // namespace rumble
