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

bool read_foes(const Level& level, const br::DepthFloor& floor, const br::Body& leader,
               std::array<br::Body, WAVE_FOES>& out) {
    for (uint32_t i = 0; i < WAVE_FOES; ++i) {
        out[i] = leader;
        out[i].pos.x = leader.pos.x + fix32::from_int(FORMATION[i].x);
        out[i].pos.z = leader.pos.z + fix32::from_int(FORMATION[i].z);
        if (on_floor(floor, out[i].pos.x, out[i].pos.z)) continue;
        std::fprintf(stderr, "neon-rumble: level %s: wave foe %u is outside the depth band or inside a wall\n",
                     level.name, i);
        return false;
    }
    return true;
}

} // namespace

bool Brawl::open(const Level& level, const Fighters& fighters, bool waves) {
    floor = br::DepthFloor{};
    pool = br::BodyPool{};
    events = br::HitEvents{};
    seats = br::Seats{};
    players = {};
    wave = Wave{};
    wave.on = waves;
    tl::ObjectTable objects;
    if (!kinds.open(level, fighters) || !level.open_objects(objects)) return false;
    const tl::ObjectMap map = objects.find(level.name);
    if (!read_floor(level, map, floor) || !read_spawns(level, map, floor, kinds, spawns)) return false;
    if (waves && !read_foes(level, floor, spawns[DUMMY], wave.foes)) return false;
    std::array<br::BrawlInput, br::SEATS> first{};
    first[PLAYER].present = true;
    br::step_seats(seats, pool, first, seat_spawns(spawns));
    if (waves) return wave.open(pool);
    return !(pool.spawn(spawns[DUMMY]) == br::EntId{});
}

// Тело с hp 0 снимается до шага, а не сразу после: удар, выбивший его, ещё попадает в отчёт тика,
// а step_seats ставит место новым телом на спавне в этом же тике. Манекен вне волны не снимается.
void Brawl::step() {
    knocked.count = 0;
    for (uint32_t i = 0; i < pool.count; ++i) {
        const br::Body& b = pool.bodies[i];
        if (b.hp <= 0 && (b.team == 0 || wave.on)) knocked.at[knocked.count++] = Knockout{b.id, b.kind};
    }
    for (uint32_t i = 0; i < knocked.count; ++i) pool.despawn(knocked.at[i].body);
    std::array<br::BrawlInput, br::SEATS> inputs{};
    for (uint32_t p = 0; p < br::SEATS; ++p) inputs[p] = players[p].input;
    br::step_seats(seats, pool, inputs, seat_spawns(spawns));
    if (wave.on) wave.refill(pool);
    for (uint32_t i = 0; i < pool.count; ++i) {
        hp_before[i] = pool.bodies[i].hp;
        react_before[i] = pool.bodies[i].react;
    }
    std::array<br::Command, br::POOL_CAPACITY> commands{};
    br::seat_commands(seats, pool, inputs, commands);
    for (uint32_t i = 0; i < pool.count; ++i) {
        const br::Body& b = pool.bodies[i];
        if (b.team == 0 && b.kind < br::SEATS)
            commands[i].strike = strike_of(players[b.kind].attack, kinds.moves[b.kind], b);
    }
    const br::BrawlWorld world{kinds.types, &floor, br::HitRules{}};
    if (wave.on) wave.think(pool, seats, world, commands);
    br::step_brawl(pool, std::span<const br::Command>(commands.data(), pool.count), world, events);
}

DrawOrder Brawl::draw_order() const {
    DrawOrder order;
    for (uint32_t i = 0; i < order.at.size(); ++i) order.at[i] = i;
    order.count = pool.count;
    // Весь массив, а не префикс длины count: GCC 13 -O3 на sort по переменной длине выдаёт ложный
    // array-bounds. Пустые слоты уходят в хвост и за count не видны.
    std::sort(order.at.begin(), order.at.end(), [&](uint32_t a, uint32_t b) {
        if (a >= pool.count || b >= pool.count) return a < pool.count && b >= pool.count;
        const br::Body& p = pool.bodies[a];
        const br::Body& q = pool.bodies[b];
        if (p.pos.z.raw != q.pos.z.raw) return p.pos.z.raw < q.pos.z.raw;
        return p.id.seq < q.id.seq;
    });
    return order;
}

uint64_t Brawl::hash() const {
    uint64_t h = br::state_hash(pool);
    framework::physics::mix_u64(h, br::seats_hash(seats));
    if (wave.on) framework::physics::mix_u64(h, wave.hash());
    return h;
}

} // namespace rumble
