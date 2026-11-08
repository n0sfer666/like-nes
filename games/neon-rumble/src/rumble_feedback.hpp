#pragma once

#include "haptic.hpp"
#include "rumble_brawl.hpp"
#include "rumble_hotseat.hpp"

namespace rumble {

inline void hit_feedback(const framework::brawl::HitEvents& events, framework::brawl::EntId body,
                         const input::PlayerAssign& devices) {
    if (body == framework::brawl::EntId{}) return;
    bool dealt = false, hurt = false;
    for (uint32_t i = 0; i < events.count; ++i) {
        const auto& event = events.at[i];
        if (!event.kept) continue;
        dealt = dealt || event.attacker == body;
        hurt = hurt || event.target == body;
    }
    if (!dealt && !hurt) return;
    if (devices.pad_slot >= 0)
        haptic::pulse({haptic::Kind::Pad, devices.pad_slot}, hurt ? 179 : 89, hurt ? 40 : 20);
    else if (devices.use_touch)
        haptic::pulse({haptic::Kind::Phone}, hurt ? 179 : 89, hurt ? 40 : 20);
}

// Вызывать после единственного живого шага, не из rollback/resim. На игрока максимум один
// импульс за тик: урон сильнее успешного своего удара. Устройства не входят в Brawl/снапшот.
inline void hit_feedback(const Brawl& brawl, const Hotseat& hotseat) {
    for (uint32_t p = 0; p < framework::brawl::SEATS; ++p) {
        const auto& seat = brawl.seats.at[p];
        if (seat.present && hotseat.lobby().present(static_cast<int>(p)))
            hit_feedback(brawl.events, seat.body, hotseat.assignment(static_cast<int>(p)));
    }
}

} // namespace rumble
