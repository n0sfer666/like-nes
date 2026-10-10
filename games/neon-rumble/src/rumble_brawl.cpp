#include "rumble_brawl.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "body_hash.hpp"
#include "brawl_step.hpp"
#include "hash_mix.hpp"
#include "object_read.hpp"
#include "rumble_level.hpp"
#include "seat_hash.hpp"
#include "seat_step.hpp"

namespace rumble {

namespace {

namespace tl = framework::tilemap;
namespace br = framework::brawl;

fix32 end(int32_t at, int32_t size) { return fix32::from_raw(fix32::sat(int64_t{at} + size)); }

bool rect_of(const tl::MapObject& o, br::FloorRect& out) {
    if (o.shape != static_cast<uint8_t>(tl::ObjectShape::Rect)) return false;
    out = {fix32::from_raw(o.x_raw), fix32::from_raw(o.y_raw), end(o.x_raw, o.w_raw), end(o.y_raw, o.h_raw)};
    return true;
}

bool read_floor(const Level& level, const tl::ObjectMap& map, br::DepthFloor& floor) {
    const auto bands = tl::objects_of_class(map, "depth_band");
    if (bands.size() != 1 || !rect_of(bands.front(), floor.band)) {
        std::fprintf(stderr, "neon-rumble: level %s needs one rectangle of class depth_band, has %zu\n",
                     level.name, bands.size());
        return false;
    }
    for (const tl::MapObject& o : tl::objects_of_class(map, "wall")) {
        br::FloorRect r;
        if (rect_of(o, r) && floor.add_wall(r)) continue;
        std::fprintf(stderr, "neon-rumble: level %s: wall %s is not a rectangle or does not fit\n", level.name,
                     tl::object_text(map, o.name_offset));
        return false;
    }
    return true;
}

int8_t facing_of(const tl::ObjectMap& map, const tl::MapObject& o) {
    const tl::ObjectProp* p = tl::object_prop(map, o, "facing");
    const bool left = p != nullptr && p->type == static_cast<uint8_t>(tl::PropType::String) &&
                      std::strcmp(tl::object_text(map, static_cast<uint32_t>(p->value)), "left") == 0;
    return left ? int8_t{-1} : int8_t{1};
}

bool inside(const br::FloorRect& r, fix32 x, fix32 z) { return !(x < r.x0) && !(r.x1 < x) && !(z < r.z0) && !(r.z1 < z); }

bool in_wall(const br::FloorRect& r, fix32 x, fix32 z) { return r.x0 < x && x < r.x1 && r.z0 < z && z < r.z1; }

bool on_floor(const br::DepthFloor& floor, fix32 x, fix32 z) {
    if (!inside(floor.band, x, z)) return false;
    for (uint32_t i = 0; i < floor.wall_count; ++i)
        if (in_wall(floor.walls[i], x, z)) return false;
    return true;
}

bool read_spawns(const Level& level, const tl::ObjectMap& map, const br::DepthFloor& floor, const Kinds& kinds,
                 std::array<br::Body, FIGHTERS>& out) {
    const auto spawns = tl::objects_of_class(map, "spawn");
    for (uint32_t i = 0; i < FIGHTERS; ++i) {
        const RosterEntry& r = ROSTER[i];
        const auto it = std::find_if(spawns.begin(), spawns.end(), [&](const tl::MapObject& o) {
            return std::strcmp(tl::object_text(map, o.name_offset), r.spawn) == 0;
        });
        if (it == spawns.end()) {
            std::fprintf(stderr, "neon-rumble: level %s has no spawn named %s for %s\n", level.name, r.spawn,
                         r.fighter);
            return false;
        }
        br::Body& b = out[i];
        b = br::Body{};
        b.pos.x = fix32::from_raw(it->x_raw);
        b.pos.z = fix32::from_raw(it->y_raw);
        b.facing = facing_of(map, *it);
        b.team = r.team;
        b.hp = kinds.hp[i];
        b.kind = static_cast<uint8_t>(i);
        b.clip = kinds.types[i].idle;
        if (!on_floor(floor, b.pos.x, b.pos.z)) {
            std::fprintf(stderr, "neon-rumble: level %s: spawn %s is outside the depth band or inside a wall\n",
                         level.name, r.spawn);
            return false;
        }
    }
    return true;
}

std::span<const br::Body, br::SEATS> seat_spawns(const std::array<br::Body, FIGHTERS>& spawns) {
    return std::span<const br::Body, br::SEATS>(spawns.data(), br::SEATS);
}

uint16_t strike_of(Attack attack, const PlayerMoves& moves, const br::Body& b) {
    const bool airborne = fix32{} < b.pos.y;
    if (attack == Attack::Punch) return airborne ? moves.jump_kick : moves.jab;
    if (attack == Attack::Cross && !airborne) return moves.cross;
    if (attack == Attack::Kick && !airborne) return b.run.dir != 0 ? moves.run_kick : moves.kick;
    if (attack == Attack::Grab && !airborne) return moves.grab;
    return br::NO_STRIKE;
}

} // namespace

bool Brawl::open(const Level& level, const Fighters& fighters) {
    floor = br::DepthFloor{};
    pool = br::BodyPool{};
    events = br::HitEvents{};
    seats = br::Seats{};
    players = {};
    tl::ObjectTable objects;
    if (!kinds.open(level, fighters) || !level.open_objects(objects)) return false;
    const tl::ObjectMap map = objects.find(level.name);
    if (!read_floor(level, map, floor) || !read_spawns(level, map, floor, kinds, spawns)) return false;
    std::array<br::BrawlInput, br::SEATS> first{};
    first[PLAYER].present = true;
    br::step_seats(seats, pool, first, seat_spawns(spawns));
    return !(pool.spawn(spawns[DUMMY]) == br::EntId{});
}

// Тело с hp 0 снимается до шага, а не сразу после: удар, выбивший его, ещё попадает в отчёт тика,
// а step_seats ставит место новым телом на спавне в этом же тике.
void Brawl::step() {
    for (const br::Seat& s : seats.at) {
        const br::Body* b = s.present ? pool.find(s.body) : nullptr;
        if (b != nullptr && b->hp <= 0) pool.despawn(s.body);
    }
    std::array<br::BrawlInput, br::SEATS> inputs{};
    for (uint32_t p = 0; p < br::SEATS; ++p) inputs[p] = players[p].input;
    br::step_seats(seats, pool, inputs, seat_spawns(spawns));
    for (uint32_t i = 0; i < FIGHTERS; ++i) {
        const br::Body* b = find(i);
        hp_before[i] = b != nullptr ? b->hp : 0;
        react_before[i] = b != nullptr ? b->react : br::Reaction::None;
    }
    std::array<br::Command, br::POOL_CAPACITY> commands{};
    br::seat_commands(seats, pool, inputs, commands);
    for (uint32_t i = 0; i < pool.count; ++i) {
        const br::Body& b = pool.bodies[i];
        if (b.kind < br::SEATS) commands[i].strike = strike_of(players[b.kind].attack, kinds.moves[b.kind], b);
    }
    const br::BrawlWorld world{kinds.types, &floor, br::HitRules{}};
    br::step_brawl(pool, std::span<const br::Command>(commands.data(), pool.count), world, events);
}

const br::Body* Brawl::find(uint32_t fighter) const {
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.bodies[i].kind == fighter) return &pool.bodies[i];
    return nullptr;
}

uint32_t Brawl::fighter_of(br::EntId id) const {
    const br::Body* b = pool.find(id);
    return b != nullptr ? b->kind : FIGHTERS;
}

DrawOrder Brawl::draw_order() const {
    DrawOrder order;
    for (uint32_t i = 0; i < FIGHTERS; ++i) {
        order.at[i] = i;
        if (find(i) != nullptr) ++order.count;
    }
    // Весь массив, а не префикс длины count: GCC 13 -O3 на sort по переменной длине выдаёт ложный
    // array-bounds. Бойцы не на улице уходят в хвост и за count не видны.
    std::sort(order.at.begin(), order.at.end(), [&](uint32_t a, uint32_t b) {
        const br::Body* p = find(a);
        const br::Body* q = find(b);
        if (p == nullptr || q == nullptr) return p != nullptr && q == nullptr;
        if (p->pos.z.raw != q->pos.z.raw) return p->pos.z.raw < q->pos.z.raw;
        return p->id.seq < q->id.seq;
    });
    return order;
}

uint64_t Brawl::hash() const {
    uint64_t h = br::state_hash(pool);
    framework::physics::mix_u64(h, br::seats_hash(seats));
    return h;
}

} // namespace rumble
