#pragma once

#include "rumble_frame_stats.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_play.hpp"

namespace rumble {

// Commands must have passed the real touch binding and the lobby's seat assignment.
inline void observe_frame_touch(FrameStatsRun& run, const Hotseat& hotseat, const Play& play) {
    const int seat = hotseat.touch_player();
    if (seat < 0 || static_cast<size_t>(seat) >= play.brawl.players.size()) return;
    const auto& command = play.brawl.players[static_cast<size_t>(seat)];
    if (!command.input.present) return;
    const auto& move = command.input.move;
    run.observe_touch(move.move_x != fix32{} || move.move_z != fix32{}, command.attack != Attack::None);
}

} // namespace rumble
