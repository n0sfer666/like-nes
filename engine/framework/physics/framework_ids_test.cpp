#include <array>
#include <cstdio>
#include <span>

#include "hash_mix.hpp"
#include "platform_args.hpp"
#include "seat_hash.hpp"
#include "world_rng.hpp"

// Спека #29 В2б: опознание тел, ГСЧ мира и места игроков — общее ядро SDK, а не жанр драки. ГСЧ
// закреплён последовательностью, хеш мест — по полю, и у обоих утверждений есть сломанная копия,
// на которой они краснеют: иначе зелёный прогон не отличим от проверки, которая ничего не видит.
namespace {

using framework::core::EntId;
using framework::core::Seat;
using framework::core::WorldRng;
using framework::physics::seats_hash;
using Seats = framework::core::Seats<>;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void test_ent_id_compares_seq() {
    check(EntId{3} == EntId{3} && EntId{3} != EntId{4}, "EntId compares by seq");
    check(EntId{} == EntId{0}, "the default EntId is seq 0, the empty one");
}

using Rng4 = std::array<uint32_t, 4>;

constexpr Rng4 PINNED{0x09f1fd9du, 0x55327416u, 0x5d5bca46u, 4u};

// Сломанная копия: без второго перемешивания. Закреплённая последовательность обязана её отбить.
uint32_t next_u32_unmixed(WorldRng& rng) {
    rng.state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = rng.state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    return static_cast<uint32_t>((z ^ (z >> 31)) >> 32);
}

template <typename Next>
Rng4 draw(Next next) {
    WorldRng r{0x5eed};
    const uint32_t a = next(r);
    const uint32_t b = next(r);
    const uint32_t c = next(r);
    return Rng4{a, b, c, below(r, 10)};
}

void test_rng_is_pinned() {
    const Rng4 got = draw([](WorldRng& r) { return next_u32(r); });
    for (uint32_t v : got) std::printf("  rng %08x\n", v);
    check(got == PINNED, "the world RNG sequence is the pinned one on every OS");
    check(draw(next_u32_unmixed) != PINNED, "control: an RNG without the second mix misses the pin");
    WorldRng z;
    for (int i = 0; i < 1000; ++i) check(below(z, 7) < 7, "below stays in range");
}

struct Field {
    const char* name;
    void (*bump)(Seats&);
    void (*erase)(Seats&);
};

const Field FIELDS[] = {
    {"present", [](Seats& s) { s.at[1].present = !s.at[1].present; },
     [](Seats& s) {
         for (Seat& x : s.at) x.present = false;
     }},
    {"body", [](Seats& s) { s.at[1].body.seq += 1; },
     [](Seats& s) {
         for (Seat& x : s.at) x.body = EntId{};
     }},
};

constexpr int FIELD_COUNT = static_cast<int>(sizeof(FIELDS) / sizeof(FIELDS[0]));

// Сломанная копия хеша: present забыт. Тест полей обязан назвать именно его.
uint64_t seats_hash_without_present(const Seats& seats) {
    uint64_t h = framework::physics::FNV_OFFSET;
    for (const Seat& s : seats.at) framework::physics::mix(h, s.body.seq);
    return h;
}

using Hash = uint64_t (*)(const Seats&);

int blind_fields(Hash hash_of, int forgotten) {
    Seats base;
    base.at = {Seat{true, EntId{2}}, Seat{true, EntId{3}}};
    const auto hash = [hash_of, forgotten](Seats s) {
        if (forgotten >= 0) FIELDS[forgotten].erase(s);
        return hash_of(s);
    };
    int blind = 0;
    for (int k = 0; k < FIELD_COUNT; ++k) {
        Seats bumped = base;
        FIELDS[k].bump(bumped);
        if (hash(bumped) == hash(base)) blind |= 1 << k;
    }
    return blind;
}

void test_the_seat_hash_sees_every_field() {
    const Hash real = [](const Seats& s) { return seats_hash(s); };
    check(blind_fields(real, -1) == 0, "a change of present or of the seat's body changes the seat hash");
    for (int k = 0; k < FIELD_COUNT; ++k)
        check(blind_fields(real, k) == 1 << k, FIELDS[k].name);
    check(blind_fields(seats_hash_without_present, -1) == 1, "control: a hash without present is blind to present");
}

using Seats3 = framework::core::Seats<3>;

// Сломанная копия перегрузки: хеширует два места из трёх. Тест третьего места обязан её отбить.
uint64_t seats_hash_first_two(const Seats3& seats) {
    return seats_hash(std::span<const Seat>(seats.at.data(), 2));
}

void test_three_seats_hash_like_their_span() {
    Seats3 three;
    three.at = {Seat{true, EntId{2}}, Seat{false, EntId{}}, Seat{true, EntId{7}}};
    const uint64_t base = seats_hash(three);
    check(base == seats_hash(std::span<const Seat>(three.at)), "Seats<3> hashes as the span of its seats");
    Seats3 moved = three;
    moved.at[2].present = false;
    check(seats_hash(moved) != base, "the third seat's present reaches the hash");
    check(seats_hash_first_two(moved) == seats_hash_first_two(three),
          "control: a hash of the first two seats misses the third seat's present");
    moved = three;
    moved.at[2].body.seq += 1;
    check(seats_hash(moved) != base, "the third seat's body reaches the hash");
    check(seats_hash_first_two(moved) == seats_hash_first_two(three),
          "control: a hash of the first two seats misses the third seat's body");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework ids: entity ids, world RNG, player seats\n");
    test_ent_id_compares_seq();
    test_rng_is_pinned();
    test_the_seat_hash_sees_every_field();
    test_three_seats_hash_like_their_span();
    std::printf("framework-ids: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
