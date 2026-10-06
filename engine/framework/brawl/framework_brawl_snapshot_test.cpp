#include "body_hash.hpp"
#include "body_pool.hpp"
#include "depth_step.hpp"
#include "framework_brawl_check.hpp"

namespace {

using namespace framework::brawl;
using test::check;

constexpr fix32 px(int32_t v) { return fix32::from_int(v); }

const DepthProfile PROFILE{px(2), px(1), fix32::from_raw(fix32::ONE / 2), px(6)};

template <typename F> void each(BodyPool& p, F f) {
    for (uint32_t i = 0; i < p.count; ++i) f(p.bodies[i]);
}

Body& last(BodyPool& p) { return p.bodies[p.count - 1]; }

struct Field {
    const char* name;
    void (*bump)(BodyPool&);
    void (*erase)(BodyPool&);
};

const Field FIELDS[] = {
    {"seq", [](BodyPool& p) { last(p).id.seq += 100; },
     [](BodyPool& p) { each(p, [](Body& b) { b.id = EntId{}; }); }},
    {"x", [](BodyPool& p) { last(p).pos.x.raw += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.pos.x = fix32{}; }); }},
    {"z", [](BodyPool& p) { last(p).pos.z.raw += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.pos.z = fix32{}; }); }},
    {"y", [](BodyPool& p) { last(p).pos.y.raw += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.pos.y = fix32{}; }); }},
    {"vx", [](BodyPool& p) { last(p).pos.vx.raw += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.pos.vx = fix32{}; }); }},
    {"vz", [](BodyPool& p) { last(p).pos.vz.raw += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.pos.vz = fix32{}; }); }},
    {"vy", [](BodyPool& p) { last(p).pos.vy.raw += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.pos.vy = fix32{}; }); }},
    {"facing", [](BodyPool& p) { last(p).facing = static_cast<int8_t>(-last(p).facing); },
     [](BodyPool& p) { each(p, [](Body& b) { b.facing = 0; }); }},
    {"team", [](BodyPool& p) { last(p).team ^= 1u; },
     [](BodyPool& p) { each(p, [](Body& b) { b.team = 0; }); }},
    {"owner", [](BodyPool& p) { last(p).owner.seq += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.owner = EntId{}; }); }},
    {"hp", [](BodyPool& p) { last(p).hp += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.hp = 0; }); }},
    {"crushed", [](BodyPool& p) { last(p).crushed = !last(p).crushed; },
     [](BodyPool& p) { each(p, [](Body& b) { b.crushed = false; }); }},
    {"age", [](BodyPool& p) { last(p).age += 1; },
     [](BodyPool& p) { each(p, [](Body& b) { b.age = 0; }); }},
    {"next_seq", [](BodyPool& p) { p.next_seq += 1; }, [](BodyPool& p) { p.next_seq = 0; }},
};

constexpr int FIELD_COUNT = static_cast<int>(sizeof(FIELDS) / sizeof(FIELDS[0]));
constexpr int NONE = -1;

uint64_t hash_forgetting(const BodyPool& pool, int forgotten) {
    if (forgotten == NONE) return state_hash(pool);
    BodyPool copy = pool;
    FIELDS[forgotten].erase(copy);
    return state_hash(copy);
}

BodyPool two_bodies() {
    BodyPool pool;
    Body a;
    a.pos = DepthBody{px(10), px(20), px(3), px(1), px(-1), px(2)};
    a.hp = 50;
    pool.spawn(a);
    Body b = a;
    b.team = 1;
    b.owner = EntId{1};
    pool.spawn(b);
    return pool;
}

int blind_field(int forgotten, int& blind_count) {
    const BodyPool base = two_bodies();
    const uint64_t h0 = hash_forgetting(base, forgotten);
    int blind = NONE;
    blind_count = 0;
    for (int k = 0; k < FIELD_COUNT; ++k) {
        BodyPool bumped = base;
        FIELDS[k].bump(bumped);
        if (hash_forgetting(bumped, forgotten) != h0) continue;
        blind = k;
        ++blind_count;
    }
    return blind;
}

void test_the_hash_sees_every_field() {
    int blind_count = 0;
    const int blind = blind_field(NONE, blind_count);
    check(blind_count == 0, "a change of any snapshot field changes the state hash");
    if (blind != NONE) std::printf("    blind to: %s\n", FIELDS[blind].name);
}

void test_a_forgotten_field_is_named() {
    for (int k = 0; k < FIELD_COUNT; ++k) {
        int blind_count = 0;
        const int blind = blind_field(k, blind_count);
        if (blind == k && blind_count == 1) continue;
        std::printf("    forgotten %s: blind to %d field(s)\n", FIELDS[k].name, blind_count);
        check(false, "a hash with one field forgotten is blind to exactly that field");
    }
}

void test_despawn_changes_the_hash() {
    BodyPool pool = two_bodies();
    const uint64_t before = state_hash(pool);
    pool.despawn(pool.bodies[0].id);
    check(state_hash(pool) != before, "a body leaving the pool changes the hash");
}

uint64_t hash_every_slot(const BodyPool& pool) {
    BodyPool copy = pool;
    copy.count = POOL_CAPACITY;
    return state_hash(copy);
}

bool dead_slots_are_unseen(uint64_t (*hash)(const BodyPool&)) {
    BodyPool pool = two_bodies();
    const uint64_t before = hash(pool);
    pool.bodies[pool.count].hp = 7;
    return hash(pool) == before;
}

void test_only_the_living_are_hashed() {
    check(dead_slots_are_unseen(state_hash), "a dead slot does not reach the hash");
    check(!dead_slots_are_unseen(hash_every_slot), "control: a hash over every slot sees the dead one");
}

BrawlInput scripted(uint32_t t) {
    BrawlInput in;
    in.present = true;
    in.move.move_x = px((t / 4) % 3 == 0 ? -1 : 1);
    in.move.move_z = fix32::from_raw(static_cast<int32_t>(t % 5) * fix32::ONE / 4 - fix32::ONE / 2);
    if (t % 7 == 0) in.buttons = button::JUMP;
    return in;
}

void run(BodyPool& pool, uint32_t from, uint32_t to) {
    DepthFloor f;
    f.band = FloorRect{px(0), px(0), px(200), px(40)};
    for (uint32_t t = from; t < to; ++t) {
        if (t == from + 5) pool.spawn(Body{});
        for (uint32_t i = 0; i < pool.count; ++i) step_body(pool.bodies[i], scripted(t + i), PROFILE, f);
    }
}

bool resim_matches(bool keep_live_seq) {
    BodyPool pool = two_bodies();
    run(pool, 0, 10);
    const BodyPool snap = pool;
    run(pool, 10, 30);
    const uint64_t straight = state_hash(pool);
    const uint32_t live_seq = pool.next_seq;
    pool = snap;
    if (keep_live_seq) pool.next_seq = live_seq;
    run(pool, 10, 30);
    return state_hash(pool) == straight;
}

void test_restore_resimulates_the_same_run() {
    check(resim_matches(false), "restoring the pool copy and replaying gives the straight-run hash");
    check(!resim_matches(true), "control: a seq counter outside the snapshot breaks the resim");
}

} // namespace

int main() {
    std::printf("brawl: the snapshot inventory of the body pool\n");
    test_the_hash_sees_every_field();
    test_a_forgotten_field_is_named();
    test_despawn_changes_the_hash();
    test_only_the_living_are_hashed();
    test_restore_resimulates_the_same_run();
    return test::verdict("framework-brawl-snapshot");
}
