#include "combat.hpp"

#include <algorithm>
#include <vector>

namespace game {
namespace {

const fix32 BULLET_VX = fix32::from_int(640);
const fix32 ENEMY_VX = fix32::from_int(-190);
constexpr uint32_t FIRE_CD = 9;      // тиков (~150мс @60Гц)
constexpr uint32_t SPAWN_CD = 46;
constexpr uint32_t SCORE_KILL = 100;

// Полу-размеры коллизий (raw fix32).
const int32_t SHIP_HW = fix32::from_int(44).raw, SHIP_HH = fix32::from_int(28).raw;
const int32_t ENEMY_HW = fix32::from_int(26).raw, ENEMY_HH = fix32::from_int(18).raw;
const int32_t BULLET_HW = fix32::from_int(11).raw, BULLET_HH = fix32::from_int(4).raw;

const fix32 BULLET_EDGE = fix32::from_int(HALF_W + 40);
const fix32 ENEMY_SPAWN_X = fix32::from_int(HALF_W + 40);
const fix32 ENEMY_KILL_X = fix32::from_int(-(HALF_W + 60));
const fix32 BULLET_MUZZLE = fix32::from_int(52);

uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
int64_t iabs(int64_t v) { return v < 0 ? -v : v; }

bool overlap(int32_t ax, int32_t ay, int32_t ahw, int32_t ahh,
             int32_t bx, int32_t by, int32_t bhw, int32_t bhh) {
    return iabs((int64_t)ax - bx) < (int64_t)ahw + bhw &&
           iabs((int64_t)ay - by) < (int64_t)ahh + bhh;
}

struct Item { flecs::entity e; int32_t x, y; uint32_t seq; bool dead = false; };

void collect(flecs::world& w, std::vector<Item>& out, bool enemies) {
    w.each([&](flecs::entity e, Transform& t, EntId& id) {
        const bool is_e = e.has<Enemy>(), is_b = e.has<Bullet>();
        if ((enemies && is_e) || (!enemies && is_b))
            out.push_back({e, t.x.raw, t.y.raw, id.seq});
    });
    std::sort(out.begin(), out.end(), [](const Item& a, const Item& b) { return a.seq < b.seq; });
}

} // namespace

void combat_step(flecs::world& world, GameState& gs, const input::InputFrame& in, fix32 dt) {
    gs.tick++;

    // Позиция корабля (для дула и коллизий). Ship — пустой тег → фильтр has<>.
    fix32 sx{}, sy{};
    world.each([&](flecs::entity e, Transform& t, Velocity&) {
        if (e.has<Ship>()) { sx = t.x; sy = t.y; }
    });

    // Стрельба (авто-огонь удержанием + кулдаун).
    if (gs.fire_cd > 0) gs.fire_cd--;
    if (in.action_held(A_Fire) && gs.fire_cd == 0) {
        world.entity().set<Transform>({sx + BULLET_MUZZLE, sy})
             .set<Velocity>({BULLET_VX, fix32{}}).add<Bullet>().set<EntId>({gs.seq++});
        gs.fire_cd = FIRE_CD;
    }

    // Волны врагов (детерм. y из LCG в целочисл. домене).
    if (gs.spawn_cd > 0) gs.spawn_cd--;
    if (gs.spawn_cd == 0) {
        const int32_t ey = (int32_t)(lcg(gs.rng) % 421u) - 210;
        world.entity().set<Transform>({ENEMY_SPAWN_X, fix32::from_int(ey)})
             .set<Velocity>({ENEMY_VX, fix32{}}).set<Enemy>({1}).set<EntId>({gs.seq++});
        gs.spawn_cd = SPAWN_CD;
    }

    // Движение пуль/врагов + despawn за краями.
    std::vector<flecs::entity> gone;
    world.each([&](flecs::entity e, Transform& t, Velocity& v) {   // Bullet — тег → фильтр
        if (!e.has<Bullet>()) return;
        t.x = t.x + v.x * dt;
        if (BULLET_EDGE < t.x) gone.push_back(e);
    });
    world.each([&](flecs::entity e, Transform& t, Enemy&) {
        t.x = t.x + ENEMY_VX * dt;
        if (t.x < ENEMY_KILL_X) gone.push_back(e);
    });

    // Коллизии в канон. порядке seq: пуля×враг → взрыв+очки; враг×корабль → −жизнь.
    std::vector<Item> enemies, bullets;
    collect(world, enemies, true);
    collect(world, bullets, false);
    for (Item& en : enemies) {
        if (en.dead) continue;
        if (overlap(en.x, en.y, ENEMY_HW, ENEMY_HH, sx.raw, sy.raw, SHIP_HW, SHIP_HH)) {
            en.dead = true;
            if (gs.lives > 0) gs.lives--;
            continue;
        }
        for (Item& b : bullets) {
            if (b.dead) continue;
            if (overlap(en.x, en.y, ENEMY_HW, ENEMY_HH, b.x, b.y, BULLET_HW, BULLET_HH)) {
                en.dead = b.dead = true;
                gs.score += SCORE_KILL;
                break;
            }
        }
    }
    for (Item& x : enemies) if (x.dead) gone.push_back(x.e);
    for (Item& x : bullets) if (x.dead) gone.push_back(x.e);

    for (flecs::entity e : gone) if (e.is_alive()) e.destruct();
}

uint64_t sim_hash(flecs::world& world, const GameState& gs) {
    // FNV-1a по GameState + сущности (Transform+EntId) в порядке seq. Порядок канонический
    // (сорт по seq) → не зависит от порядка обхода flecs.
    std::vector<Item> ents;
    world.each([&](flecs::entity e, Transform& t, EntId& id) {
        ents.push_back({e, t.x.raw, t.y.raw, id.seq});
    });
    std::sort(ents.begin(), ents.end(), [](const Item& a, const Item& b) { return a.seq < b.seq; });

    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix(gs.tick); mix(gs.seq); mix(gs.score); mix((uint32_t)gs.lives);
    mix(gs.rng); mix(gs.fire_cd); mix(gs.spawn_cd);
    for (const Item& it : ents) {
        uint32_t kind = it.e.has<Enemy>() ? 2u : (it.e.has<Bullet>() ? 1u : 0u);
        mix(it.seq); mix(kind); mix((uint32_t)it.x); mix((uint32_t)it.y);
        if (kind == 2u) mix((uint32_t)it.e.get<Enemy>().hp);
    }
    return h;
}

} // namespace game
