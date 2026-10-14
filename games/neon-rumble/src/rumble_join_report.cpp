#include "rumble_join_report.hpp"

#include <cstdio>

#include "rumble_label.hpp"
#include "viewport_fit.hpp"

namespace rumble {

void report_joins(const Brawl& brawl, const framework::brawl::Seats& was, uint32_t tick) {
    for (uint32_t p = 0; p < framework::brawl::SEATS; ++p) {
        if (was.at[p].present || !brawl.seats.at[p].present) continue;
        const framework::brawl::Body* b = brawl.pool.find(brawl.seats.at[p].body);
        if (b == nullptr) continue;
        std::printf("neon-rumble: join tick %u: P%u as %s at %d,%d y %d immune %u", tick, p + 1,
                    label(brawl, *b).text.data(), b->pos.x.to_int(), b->pos.z.to_int(), b->pos.y.to_int(), b->immune);
        const Wave& w = brawl.wave;
        if (w.on) {
            const fix32 half = framework::graphics::view_zone_half().x;
            std::printf(", view %d..%d, %s", (w.camera.center.x - half).to_int(), (w.camera.center.x + half).to_int(),
                        w.sections.find(b->id) != nullptr ? "platform" : "belt");
        }
        std::printf("\n");
        std::fflush(stdout);
    }
}

} // namespace rumble
