#include "combat.hpp"
#include "boss.hpp"

#include <algorithm>
#include <vector>

namespace game {
namespace {

const fix32 BULLET_VX = fix32::from_int(640);
const fix32 ENEMY_VX = fix32::from_int(-190);
constexpr uint32_t FIRE_CD = 9, SPAWN_CD = 46;
constexpr uint32_t SCORE_KILL = 100, SCORE_BOSS = 2000, SCORE_BOSS_HIT = 10;
constexpr uint32_t BOSS_TRIGGER_KILLS = 10, BOSS_TRIGGER_T = 540;

const int32_t SHIP_HW = fix32::from_int(44).raw, SHIP_HH = fix32::from_int(28).raw;
const int32_t ENEMY_HW = fix32::from_int(26).raw, ENEMY_HH = fix32::from_int(18).raw;
const int32_t BULLET_HW = fix32::from_int(11).raw, BULLET_HH = fix32::from_int(4).raw;
const int32_t HOSTILE_HW = fix32::from_int(9).raw, HOSTILE_HH = fix32::from_int(5).raw;
const int32_t BOSS_HW = fix32::from_int(58).raw, BOSS_HH = fix32::from_int(38).raw;

const fix32 EDGE_R = fix32::from_int(HALF_W + 40);
const fix32 EDGE_L = fix32::from_int(-(HALF_W + 60));
const fix32 ENEMY_SPAWN_X = fix32::from_int(HALF_W + 40);
const fix32 MUZZLE = fix32::from_int(52);

uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
int64_t iabs(int64_t v) { return v < 0 ? -v : v; }

bool overlap(int32_t ax, int32_t ay, int32_t ahw, int32_t ahh,
             int32_t bx, int32_t by, int32_t bhw, int32_t bhh) {
    return iabs((int64_t)ax - bx) < (int64_t)ahw + bhw &&
           iabs((int64_t)ay - by) < (int64_t)ahh + bhh;
}

struct Item { flecs::entity e; int32_t x, y; uint32_t seq; bool dead = false; };

void bucket(flecs::world& w, std::vector<Item>& bul, std::vector<Item>& ene, std::vector<Item>& hos) {
    w.each([&](flecs::entity e, Transform& t, EntId& id) {
        Item it{e, t.x.raw, t.y.raw, id.seq};
        if (e.has<Bullet>()) bul.push_back(it);
        else if (e.has<Enemy>()) ene.push_back(it);
        else if (e.has<Hostile>()) hos.push_back(it);
    });
    auto by = [](const Item& a, const Item& b) { return a.seq < b.seq; };
    std::sort(bul.begin(), bul.end(), by);
    std::sort(ene.begin(), ene.end(), by);
    std::sort(hos.begin(), hos.end(), by);
}

void fxpush(FxSink* fx, int32_t rawx, int32_t rawy, uint8_t k) {
    if (fx) fx->events.push_back({rawx / 65536.0f, rawy / 65536.0f, k});
}

// Арена: огонь игрока + [спавн врагов] + движение всех снарядов/врагов + коллизии + despawn.
void arena(flecs::world& world, GameState& gs, const input::InputFrame& in, fix32 dt,
           fix32 sx, fix32 sy, bool spawn_enemies, FxSink* fx) {
    if (gs.fire_cd > 0) gs.fire_cd--;
    if (in.action_held(A_Fire) && gs.fire_cd == 0) {
        world.entity().set<Transform>({sx + MUZZLE, sy}).set<Velocity>({BULLET_VX, fix32{}})
             .add<Bullet>().set<EntId>({gs.seq++});
        gs.fire_cd = FIRE_CD;
        fxpush(fx, (sx + MUZZLE).raw, sy.raw, FX_Fire);
    }
    if (spawn_enemies) {
        if (gs.spawn_cd > 0) gs.spawn_cd--;
        if (gs.spawn_cd == 0) {
            const int32_t ey = (int32_t)(lcg(gs.rng) % 421u) - 210;
            world.entity().set<Transform>({ENEMY_SPAWN_X, fix32::from_int(ey)})
                 .set<Velocity>({ENEMY_VX, fix32{}}).set<Enemy>({1}).set<EntId>({gs.seq++});
            gs.spawn_cd = SPAWN_CD;
        }
    }

    std::vector<flecs::entity> gone;
    world.each([&](flecs::entity e, Transform& t, Velocity& v) {
        if (e.has<Bullet>()) { t.x = t.x + v.x * dt; if (EDGE_R < t.x) gone.push_back(e); }
        else if (e.has<Hostile>()) {
            t.x = t.x + v.x * dt; t.y = t.y + v.y * dt;
            if (t.x < EDGE_L) gone.push_back(e);
        }
    });
    world.each([&](flecs::entity e, Transform& t, Enemy&) {
        t.x = t.x + ENEMY_VX * dt;
        if (t.x < EDGE_L) gone.push_back(e);
    });

    // Босс (для bullet×boss).
    flecs::entity boss{}; int32_t bx = 0, by = 0;
    world.each([&](flecs::entity e, Transform& t, Boss&) { boss = e; bx = t.x.raw; by = t.y.raw; });

    std::vector<Item> bul, ene, hos;
    bucket(world, bul, ene, hos);
    // пуля×враг → очки/kills; пуля×босс → −HP.
    for (Item& b : bul) {
        if (b.dead) continue;
        for (Item& en : ene) {
            if (en.dead) continue;
            if (overlap(en.x, en.y, ENEMY_HW, ENEMY_HH, b.x, b.y, BULLET_HW, BULLET_HH)) {
                en.dead = b.dead = true; gs.score += SCORE_KILL; gs.kills++;
                gone.push_back(en.e); fxpush(fx, en.x, en.y, FX_EnemyDie); break;
            }
        }
        if (b.dead || !boss.is_alive()) continue;
        if (overlap(bx, by, BOSS_HW, BOSS_HH, b.x, b.y, BULLET_HW, BULLET_HH)) {
            b.dead = true;
            Boss bs = boss.get<Boss>();
            if (bs.hp <= 0) continue;              // уже мёртв в этом тике — без инфляции очков
            bs.hp--; boss.set<Boss>(bs);
            gs.score += SCORE_BOSS_HIT;
            fxpush(fx, b.x, b.y, FX_BossHit);
            if (bs.hp <= 0) { gone.push_back(boss); fxpush(fx, bx, by, FX_BossDie); }
        }
    }
    // враг×корабль, hostile×корабль → −жизнь.
    for (Item& en : ene) {
        if (en.dead) continue;
        if (overlap(en.x, en.y, ENEMY_HW, ENEMY_HH, sx.raw, sy.raw, SHIP_HW, SHIP_HH)) {
            en.dead = true; if (gs.lives > 0) gs.lives--; gone.push_back(en.e);
            fxpush(fx, sx.raw, sy.raw, FX_PlayerHit);
        }
    }
    for (Item& h : hos) {
        if (h.dead) continue;
        if (overlap(h.x, h.y, HOSTILE_HW, HOSTILE_HH, sx.raw, sy.raw, SHIP_HW, SHIP_HH)) {
            h.dead = true; if (gs.lives > 0) gs.lives--; gone.push_back(h.e);
            fxpush(fx, sx.raw, sy.raw, FX_PlayerHit);
        }
    }
    for (Item& b : bul) if (b.dead) gone.push_back(b.e);
    for (flecs::entity e : gone) if (e.is_alive()) e.destruct();
}

void clear_combat(flecs::world& world) {
    std::vector<flecs::entity> gone;
    world.each([&](flecs::entity e, EntId&) {
        if (e.has<Bullet>() || e.has<Enemy>() || e.has<Hostile>() || e.has<Boss>())
            gone.push_back(e);
    });
    for (flecs::entity e : gone) if (e.is_alive()) e.destruct();
}

} // namespace

void combat_step(flecs::world& world, GameState& gs, const input::InputFrame& in, fix32 dt,
                 FxSink* fx) {
    gs.tick++;
    fix32 sx{}, sy{};
    world.each([&](flecs::entity e, Transform& t, Velocity&) {
        if (e.has<Ship>()) { sx = t.x; sy = t.y; }
    });

    switch (gs.phase) {
        case PH_Intro:
            if (in.action_pressed(A_Fire)) { gs.phase = PH_Play; gs.phase_t = 0; }
            break;
        case PH_Play:
            arena(world, gs, in, dt, sx, sy, true, fx);
            if (gs.kills >= BOSS_TRIGGER_KILLS || gs.phase_t >= BOSS_TRIGGER_T) {
                clear_combat(world); boss_spawn(world, gs); gs.phase = PH_Boss; gs.phase_t = 0;
            }
            break;
        case PH_Boss: {
            boss_step(world, gs, dt, sy);
            arena(world, gs, in, dt, sx, sy, false, fx);
            bool alive = false;
            world.each([&](Boss&) { alive = true; });
            if (!alive) { gs.score += SCORE_BOSS; gs.phase = PH_Victory; gs.phase_t = 0; }
            break;
        }
        case PH_Victory:
        case PH_GameOver:
            if (gs.phase_t > 40 && in.action_pressed(A_Fire)) reset_run(world, gs);
            break;
    }
    if ((gs.phase == PH_Play || gs.phase == PH_Boss) && gs.lives <= 0) {
        gs.phase = PH_GameOver; gs.phase_t = 0;
    }
    gs.phase_t++;
}

void reset_run(flecs::world& world, GameState& gs) {
    clear_combat(world);
    world.each([&](flecs::entity e, Transform& t, Velocity&) {
        if (e.has<Ship>()) { t.x = fix32::from_int(-300); t.y = fix32{}; }
    });
    gs.rng = 0x1234567u; gs.fire_cd = 0; gs.spawn_cd = 0; gs.score = 0; gs.lives = 3;
    gs.kills = 0; gs.phase = PH_Play; gs.phase_t = 0;
}

uint64_t sim_hash(flecs::world& world, const GameState& gs) {
    std::vector<Item> ents;
    world.each([&](flecs::entity e, Transform& t, EntId& id) {
        ents.push_back({e, t.x.raw, t.y.raw, id.seq});
    });
    std::sort(ents.begin(), ents.end(), [](const Item& a, const Item& b) { return a.seq < b.seq; });

    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix(gs.tick); mix(gs.seq); mix(gs.score); mix((uint32_t)gs.lives); mix(gs.rng);
    mix(gs.fire_cd); mix(gs.spawn_cd); mix(gs.phase); mix(gs.phase_t); mix(gs.kills);
    for (const Item& it : ents) {
        uint32_t kind = it.e.has<Enemy>() ? 2u : it.e.has<Bullet>() ? 1u
                        : it.e.has<Hostile>() ? 3u : it.e.has<Boss>() ? 4u : 0u;
        mix(it.seq); mix(kind); mix((uint32_t)it.x); mix((uint32_t)it.y);
        if (kind == 2u) mix((uint32_t)it.e.get<Enemy>().hp);
        if (kind == 4u) mix((uint32_t)it.e.get<Boss>().hp);
    }
    return h;
}

} // namespace game
