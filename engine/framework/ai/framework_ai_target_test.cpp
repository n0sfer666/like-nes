#include <array>

#include "ai_target.hpp"
#include "framework_ai_target_test.hpp"
#include "framework_brawl_check.hpp"

namespace framework::ai::test {

namespace {

using brawl::Body;
using brawl::BodyPool;
using brawl::EntId;
using brawl::test::check;

Body at(int32_t x, uint8_t team, uint8_t kind) {
    Body b;
    b.pos.x = fix32::from_int(x);
    b.team = team;
    b.kind = kind;
    b.hp = 100;
    return b;
}

} // namespace

// Меньший kind стоит в пуле позже: правило «первый найденный» выбрало бы другого.
void test_nearest_body() {
    std::array<brawl::Archetype, 5> kinds{};
    kinds[3].role = brawl::Role::Prop;
    kinds[4].role = brawl::Role::Projectile;
    BodyPool pool;
    const EntId foe = pool.spawn(at(300, 1, 2));
    const EntId high = pool.spawn(at(260, 0, 1));
    const EntId low = pool.spawn(at(340, 0, 0));
    const Body& from = *pool.find(foe);
    check(nearest_body(pool, kinds, from, 0)->id == low, "a tie in |dx| + |dz| goes to the lower kind, not the earlier body");
    pool.find(high)->pos.x = fix32::from_int(270);
    check(nearest_body(pool, kinds, from, 0)->id == high, "the nearer body wins over the lower kind");
    for (const uint8_t kind : {uint8_t{3}, uint8_t{4}}) {
        const EntId inert = pool.spawn(at(300, 0, kind));
        check(nearest_body(pool, kinds, from, 0)->id == high, "a prop or a shot of the team is a target");
        pool.despawn(inert);
    }
    pool.find(high)->hp = 0;
    check(nearest_body(pool, kinds, from, 0)->id == low, "a body with hp 0 is not a target");
    pool.find(low)->hp = 0;
    check(nearest_body(pool, kinds, from, 0) == nullptr, "no living body of the team gives no target");
    check(nearest_body(pool, kinds, *pool.find(low), 1)->id == foe, "the team argument picks the side");
}

} // namespace framework::ai::test
