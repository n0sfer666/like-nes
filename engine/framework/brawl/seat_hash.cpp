#include "seat_hash.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::brawl {

namespace {

static_assert(std::is_trivially_copyable_v<Seats>, "the snapshot is a plain copy of the seats");
static_assert([] {
    [[maybe_unused]] auto [present, body] = Seat{};
    [[maybe_unused]] auto [at] = Seats{};
    return true;
}(), "a seat field was added: mix it here and add its row to framework_brawl_seat_test");

} // namespace

uint64_t seats_hash(const Seats& seats) {
    uint64_t h = physics::FNV_OFFSET;
    for (const Seat& s : seats.at) {
        physics::mix(h, s.present ? 1u : 0u);
        physics::mix(h, s.body.seq);
    }
    return h;
}

} // namespace framework::brawl
