#include "boss.hpp"

namespace game {
namespace {

const fix32 BOSS_HOME_X = fix32::from_int(360);   // куда босс выезжает справа
const fix32 BOSS_VX_IN = fix32::from_int(-150);   // скорость въезда
const fix32 BOSS_VY = fix32::from_int(150);       // верт. осцилляция
const fix32 BOSS_Y_LIMIT = fix32::from_int(190);
const fix32 HOSTILE_VX = fix32::from_int(-380);
const fix32 MUZZLE = fix32::from_int(-64);
constexpr uint32_t BOSS_FIRE_CD = 34;

} // namespace

void boss_spawn(flecs::world& world, GameState& gs) {
    world.entity()
        .set<Transform>({fix32::from_int(HALF_W + 80), fix32{}})
        .set<Velocity>({BOSS_VX_IN, fix32{}})
        .set<Boss>({BOSS_HP_MAX, BOSS_FIRE_CD, 1})
        .set<EntId>({gs.seq++});
}

// Движение босса (въезд → верт. осцилляция у BOSS_HOME_X) + периодический выстрел к игроку.
// Hostile-снаряд создаётся ПОСЛЕ each (не мутируем таблицы во время итерации).
void boss_step(flecs::world& world, GameState& gs, fix32 dt, fix32 player_y) {
    bool fire = false;
    fix32 fx{}, fy{}, fvy{};
    world.each([&](flecs::entity, Transform& t, Boss& b) {
        if (BOSS_HOME_X < t.x) {                       // ещё въезжает
            t.x = t.x + BOSS_VX_IN * dt;
            return;
        }
        t.x = BOSS_HOME_X;
        t.y = t.y + fix32::from_int(b.dir) * BOSS_VY * dt;   // осцилляция
        if (BOSS_Y_LIMIT < t.y) { t.y = BOSS_Y_LIMIT; b.dir = -1; }
        if (t.y < -BOSS_Y_LIMIT) { t.y = -BOSS_Y_LIMIT; b.dir = 1; }
        if (b.fire_cd > 0) b.fire_cd--;
        if (b.fire_cd == 0) {                          // выстрел к текущей высоте игрока
            fvy = (player_y < t.y) ? fix32::from_int(-120) : fix32::from_int(120);
            if (t.y < player_y + fix32::from_int(20) && player_y < t.y + fix32::from_int(20))
                fvy = fix32{};
            fx = t.x + MUZZLE; fy = t.y; fire = true;
            b.fire_cd = BOSS_FIRE_CD;
        }
    });
    if (fire)
        world.entity().set<Transform>({fx, fy}).set<Velocity>({HOSTILE_VX, fvy})
             .add<Hostile>().set<EntId>({gs.seq++});
}

} // namespace game
