#include "archetype_chain.hpp"

#include <cstring>

#include "fighter_name.hpp"

namespace framework::brawl {

uint32_t cancel_tick(const graphics::ClipView& view) {
    uint32_t t = 0;
    for (uint16_t f = 0; f < view.clip.frame_count; ++f) {
        if (std::strcmp(graphics::clip_event_name(view, view.clip.frames[f].event), CANCEL_EVENT) == 0) return t;
        t += view.clip.frames[f].duration;
    }
    return NO_CANCEL;
}

bool make_chain(const FighterTable& fighter, const graphics::ClipTable& clips, Archetype& out, std::string& error) {
    out.buffer_ticks = ticks16(fighter.buffer_ticks());
    out.chain_count = static_cast<uint8_t>(fighter.chain_count());
    for (uint32_t i = 0; i < out.chain_count; ++i) {
        const MoveSlot& m = out.moves[fighter.chain_move(i)];
        out.chain[i] = m.head;
        out.cancel[i] = cancel_tick(clips.clip(m.clip));
        if (i + 1 == out.chain_count || out.cancel[i] != NO_CANCEL) continue;
        error = no_cancel_error(i, clips.name(m.clip));
        return false;
    }
    return true;
}

} // namespace framework::brawl
