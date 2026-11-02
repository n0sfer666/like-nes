#include "seat_step.hpp"

namespace framework::brawl {

void step_seats(Seats& seats, BodyPool& pool, std::span<const BrawlInput, SEATS> inputs,
                std::span<const Body, SEATS> spawns) {
    for (uint32_t p = 0; p < SEATS; ++p) {
        Seat& s = seats.at[p];
        if (s.present && pool.find(s.body) == nullptr) s = Seat{};
        if (inputs[p].present == s.present) continue;
        if (s.present) {
            pool.despawn(s.body);
            s = Seat{};
            continue;
        }
        const EntId id = pool.spawn(spawns[p]);
        if (!(id == EntId{})) s = Seat{true, id};
    }
}

void seat_commands(const Seats& seats, const BodyPool& pool, std::span<const BrawlInput, SEATS> inputs,
                   std::span<Command> out) {
    for (uint32_t i = 0; i < pool.count && i < out.size(); ++i) {
        out[i] = Command{};
        for (uint32_t p = 0; p < SEATS; ++p)
            if (seats.at[p].present && seats.at[p].body == pool.bodies[i].id) out[i].input = inputs[p];
    }
}

} // namespace framework::brawl
