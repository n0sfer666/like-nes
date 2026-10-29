#include "seat_hash.hpp"

#include <type_traits>

#include "hash_mix.hpp"

namespace framework::physics {

namespace {

static_assert(std::is_trivially_copyable_v<core::Seats<>>, "the snapshot is a plain copy of the seats");
static_assert([] {
    [[maybe_unused]] auto [present, body] = core::Seat{};
    [[maybe_unused]] auto [at] = core::Seats<>{};
    return true;
}(), "a seat field was added: mix it here and add its row to framework_ids_test");

} // namespace

uint64_t seats_hash(std::span<const core::Seat> seats) {
    uint64_t h = FNV_OFFSET;
    for (const core::Seat& s : seats) {
        mix(h, s.present ? 1u : 0u);
        mix(h, s.body.seq);
    }
    return h;
}

} // namespace framework::physics
