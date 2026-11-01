#include "framework_kinematic_fixture.hpp"
#include "movers_hash.hpp"
#include "movers_snapshot.hpp"

#include <array>
#include <type_traits>

#include "hash_mix.hpp"
#include "shape.hpp"

namespace {

using namespace framework;
using namespace framework::kinematic;
using namespace framework::kinematic::test;
const fix32 DT = px(1) / px(60);

std::array<scene::Path, 2> paths() {
    return {path({{px(0), px(0)}, {px(40), px(0)}}, 3),
            path({{px(200), px(100)}, {px(200), px(140)}}, 2)};
}

void advance(Movers& m, std::span<const scene::Path> p, int ticks) {
    for (int i = 0; i < ticks; ++i) step_movers(m, p, DT);
}

bool reaches_targets(bool step_world) {
    const auto p = paths();
    Movers m(p);
    for (int t = 0; t < 48; ++t) {
        if (step_world) step_movers(m, p, DT);
        else for (uint32_t i = 0; i < p.size(); ++i) advance_path(p[i], m.follow[i]);
        for (uint32_t i = 0; i < p.size(); ++i) {
            const physics::Body& b = m.world.body(physics::BodyId{i});
            const Vec2 goal = path_point(p[i], m.follow[i]);
            if (abs_fix(b.position.x - goal.x).raw > 1 || abs_fix(b.position.y - goal.y).raw > 1) return false;
        }
    }
    return true;
}

enum class Forget { Nothing, World, Follow };

bool resim(Forget forget) {
    const auto p = paths();
    Movers m(p);
    physics::BodyDesc body;
    body.key = 90;
    body.position = {px(500), px(500)};
    body.shape = physics::box(px(1), px(1));
    const physics::BodyId id = m.world.add(body);
    m.world.mutate(id).velocity = {px(60), fix32{}};
    advance(m, p, 7);
    MoversSnapshot saved;
    save_movers(m, saved);
    advance(m, p, 11);
    const uint64_t want = movers_hash(m);
    if (forget == Forget::Nothing) restore_movers(saved, m);
    if (forget == Forget::World) m.follow = saved.follow;
    if (forget == Forget::Follow) saved.world.apply(m.world);
    advance(m, p, 11);
    return movers_hash(m) == want;
}

uint64_t without_direction(const Movers& m) {
    uint64_t h = physics::FNV_OFFSET;
    physics::mix_u64(h, m.world.hash());
    physics::mix(h, static_cast<uint32_t>(m.follow.size()));
    for (const PathFollow& f : m.follow) {
        physics::mix(h, f.segment);
        physics::mix(h, static_cast<uint32_t>(f.traveled.raw));
    }
    return h;
}

bool sees_direction(uint64_t (*hash)(const Movers&)) {
    const auto p = paths();
    Movers m(p);
    const uint64_t before = hash(m);
    m.follow[0].direction = -1;
    return hash(m) != before;
}

void fields() {
    const auto p = paths();
    Movers m(p);
    const uint64_t base = movers_hash(m);
    m.follow[0].segment = 1;
    check(movers_hash(m) != base, "hash sees the segment");
    m.follow[0] = {};
    m.follow[0].traveled = fix32::from_raw(1);
    check(movers_hash(m) != base, "hash sees one raw of traveled distance");
    m.follow[0] = {};
    m.world.mutate(physics::BodyId{0}).position.x = px(1);
    check(movers_hash(m) != base, "hash sees the physics world");
    m.world.mutate(physics::BodyId{0}).position.x = fix32{};
    m.follow.pop_back();
    check(movers_hash(m) != base, "hash sees the follow count");
}

} // namespace

int main() {
    static_assert(!std::is_copy_constructible_v<Movers> && !std::is_move_constructible_v<Movers>);
    const auto p = paths();
    Movers m(p);
    check(m.follow.size() == 2 && m.world.body(physics::BodyId{0}).key == 1 &&
          m.world.body(physics::BodyId{1}).key == 2 &&
          m.world.body(physics::BodyId{1}).type == physics::BodyType::Kinematic,
          "input order determines body keys and indices");
    check(reaches_targets(true), "each step reaches the absolute path target within one raw rounding unit");
    check(!reaches_targets(false), "control: advancing follow without stepping the world leaves bodies behind");
    check(resim(Forget::Nothing) && !resim(Forget::World) && !resim(Forget::Follow),
          "snapshot needs both the physics world and follow state for an identical resim");
    check(sees_direction(movers_hash) && !sees_direction(without_direction), "control: a hash without direction is detected");
    fields();
    Movers empty(std::span<const scene::Path>{});
    step_movers(empty, {}, DT);
    check(empty.follow.empty(), "zero paths step an empty world");
    return verdict("framework-kinematic-movers");
}
