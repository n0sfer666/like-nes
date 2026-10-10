#include <array>

#include "framework_brawl_check.hpp"
#include "framework_brawl_seat_fixture.hpp"

// Спека #25 В4в: место игрока в снапшоте и хеше драки. present ставит и снимает тело, Input{} —
// отсутствие, ввод игрока доходит только до его тела, и ресим со входом посреди уровня совпадает.
namespace {

using namespace framework::brawl;
using test::check;
using test::Hotseat;
using test::walking;

using Row = std::array<BrawlInput, SEATS>;

void test_default_input_is_absent() {
    Hotseat h;
    const Row none{};
    h.step(none.data());
    check(h.pool.count == 1 && !h.seats.at[0].present && !h.seats.at[1].present,
          "Input{} seats nobody: before a player's first input the ring answers with it");
}

void test_present_spawns_and_absence_removes() {
    Hotseat h;
    const Row p1{walking(1), BrawlInput{}};
    h.step(p1.data());
    const Body* b1 = h.pool.find(h.seats.at[0].body);
    check(h.seats.at[0].present && b1 != nullptr && fix32::from_int(100) < b1->pos.x && !h.seats.at[1].present,
          "P1's body appears at its spawn and walks on the tick it became present");
    const Row both{walking(1), walking(-1)};
    h.step(both.data());
    const EntId first = h.seats.at[1].body;
    const fix32 joined_x = h.pool.find(first)->pos.x;
    check(h.pool.count == 3 && h.pool.find(first) != nullptr, "P2's body joins mid-level");
    for (int t = 0; t < 5; ++t) h.step(both.data());
    h.step(p1.data());
    check(h.pool.count == 2 && h.pool.find(first) == nullptr && !h.seats.at[1].present &&
              h.seats.at[1].body == EntId{},
          "P2 leaving takes the body off the field and clears the seat");
    h.step(both.data());
    const Body* again = h.pool.find(h.seats.at[1].body);
    check(again != nullptr && !(again->id == first) && again->pos.x == joined_x,
          "a rejoin is a fresh body at the spawn, not the old one where it stood");
}

void test_a_full_pool_seats_on_the_first_free_tick() {
    Hotseat h;
    while (h.pool.count < POOL_CAPACITY) h.pool.spawn(test::dummy(0, 0, 1, 1, h.arena.kinds[0]));
    const Row p1{walking(1), BrawlInput{}};
    h.step(p1.data());
    check(!h.seats.at[0].present && h.pool.count == POOL_CAPACITY, "a full pool leaves P1 unseated, not on a ghost");
    check(h.pool.despawn(h.pool.bodies[1].id), "a body leaves the full pool");
    h.step(p1.data());
    check(h.seats.at[0].present && h.pool.find(h.seats.at[0].body) != nullptr, "P1 is seated once there is room");
}

void test_a_body_removed_elsewhere_is_replaced() {
    Hotseat h;
    const Row p1{walking(1), BrawlInput{}};
    h.step(p1.data());
    const EntId knocked = h.seats.at[0].body;
    check(h.pool.despawn(knocked), "the game takes P1's body off the field");
    h.step(p1.data());
    check(h.seats.at[0].present && !(h.seats.at[0].body == knocked) && h.pool.find(h.seats.at[0].body) != nullptr,
          "a present seat whose body is gone gets a fresh one, not input sent nowhere");
}

void test_input_reaches_only_the_seated_body() {
    Hotseat h;
    const Row both{walking(1), walking(-1)};
    h.step(both.data());
    const Row p2{BrawlInput{}, walking(-1)};
    h.step(p2.data());
    h.step(both.data());
    std::array<Command, POOL_CAPACITY> out{};
    out.fill(Command{walking(1, button::ATTACK), 0});
    seat_commands(h.seats, h.pool, both, out);
    const Body& tail = h.pool.bodies[h.pool.count - 1];
    check(tail.id == h.seats.at[0].body && out[h.pool.count - 1].input.move.move_x == fix32::from_int(1),
          "P1 rejoined behind P2 in the pool and still gets P1's input");
    check(out[0].input == BrawlInput{} && out[0].strike == NO_STRIKE, "the dummy gets an empty command");
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

int blind_fields(int forgotten) {
    Seats base;
    base.at = {Seat{true, EntId{2}}, Seat{true, EntId{3}}};
    const auto hash = [forgotten](Seats s) {
        if (forgotten >= 0) FIELDS[forgotten].erase(s);
        return seats_hash(s);
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
    check(blind_fields(-1) == 0, "a change of present or of the seat's body changes the seat hash");
    for (int k = 0; k < FIELD_COUNT; ++k)
        check(blind_fields(k) == 1 << k, FIELDS[k].name);
}

Row script(uint32_t t) {
    const BrawlInput p1 = walking(t % 9 < 5 ? 1 : -1, t % 13 == 0 ? button::ATTACK : 0);
    const bool p2 = (t >= 20 && t < 30) || t >= 35;
    return Row{p1, p2 ? walking(-1, t % 11 == 0 ? button::ATTACK : 0) : BrawlInput{}};
}

void run(Hotseat& h, uint32_t from, uint32_t to) {
    for (uint32_t t = from; t < to; ++t) {
        const Row r = script(t);
        h.step(r.data());
    }
}

bool resim_matches(bool keep_live_seats) {
    Hotseat h;
    run(h, 0, 25);
    const Hotseat::Snap snap = h.save();
    run(h, 25, 50);
    const uint64_t straight = h.hash();
    const Seats live = h.seats;
    h.restore(snap);
    if (keep_live_seats) h.seats = live;
    run(h, 25, 50);
    return h.hash() == straight;
}

void test_restore_resimulates_a_join_and_a_leave() {
    check(resim_matches(false), "restoring pool and seats replays a mid-level join, leave and rejoin");
    check(!resim_matches(true), "control: seats outside the snapshot break the resim");
}

} // namespace

int main() {
    std::printf("brawl: hotseat seats in the snapshot\n");
    test_default_input_is_absent();
    test_present_spawns_and_absence_removes();
    test_a_full_pool_seats_on_the_first_free_tick();
    test_a_body_removed_elsewhere_is_replaced();
    test_input_reaches_only_the_seated_body();
    test_the_seat_hash_sees_every_field();
    test_restore_resimulates_a_join_and_a_leave();
    return test::verdict("framework-brawl-seat");
}
