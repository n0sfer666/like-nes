#pragma once
#include "atlas_regions.hpp"
#include "particles.hpp"
#include "sprite_out.hpp"

// Описания частиц шутера ДАННЫМИ (спека #17, вертикаль 3, шаг B3). Отдельным заголовком по тому же
// основанию, что и `framework_graphics_particle_scene.hpp`: таблицу читают и игра, и гейт, а
// написанная в каждом заново она сравнивала бы разные таблицы, ничего об этом не сказав.
//
// Числа взяты из старого `example_ugly_game/fx.cpp` и переведены ДВУМЯ пересчётами, оба несущие:
// скорость и время игра-образец писала в СЕКУНДАХ, фреймворк считает тиками, а размер там был
// стороной квада, здесь — полуразмером. Пересчёт живёт функцией, а не восемью выписанными числами:
// восемь копий одного деления на шестьдесят разъезжаются на первой же правке.
namespace game {

enum FxDesc : uint16_t {
    FXD_Fire = 0,
    FXD_EnemyDie,
    FXD_BossHit,
    FXD_BossDieCore,
    FXD_BossDieFlash,
    FXD_PlayerHit,
    FXD_ShipTrail,
    FXD_BulletTrail,
    FXD_Count
};

constexpr float FX_TICKS = 60.0f;

inline uint32_t fx_rgba(float r, float g, float b, float a) {
    auto q = [](float v) {
        const int32_t i = static_cast<int32_t>(v * 255.0f + 0.5f);
        return static_cast<uint32_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
    };
    return (q(r) << 24) | (q(g) << 16) | (q(b) << 8) | q(a);
}

// Взрыв: круг во все стороны, трение 0.9 за тик, жизнь 0.28…0.83 с, размер 0.6…1.3 от заказанного и
// затухающий до 0.4 к концу. Разброс жизни и размера — та самая пара, ради которой у описания
// появились `life_jitter`/`half_jitter`: без неё облако гаснет одним диском.
inline framework::graphics::EmitDesc fx_burst(float spd, float r, float g, float b, float size) {
    framework::graphics::EmitDesc d;
    d.speed_min = fix32::from_float(0.35 * spd / FX_TICKS);
    d.speed_max = fix32::from_float(spd / FX_TICKS);
    d.spread_turns = fix32::from_float(0.5);
    d.damping = fix32::from_float(0.9);
    d.life_ticks = 50;
    d.life_jitter = fix32::from_float(1.0 - 0.28 * FX_TICKS / 50.0);
    d.half_start = fix32::from_float(size * 1.3 / 2.0);
    d.half_end = fix32::from_float(size * 1.3 * 0.4 / 2.0);
    d.half_jitter = fix32::from_float(1.0 - 0.6 / 1.3);
    d.rgba_start = fx_rgba(r, g, b, 1.0f);
    d.rgba_end = fx_rgba(r, g, b, 0.0f);
    d.region = RID_Star;
    d.material = MAT_Glow;
    return d;
}

// След: узкая струя назад. Полоса рождения (`spawn_half`) здесь не украшение — выхлоп корабля
// выходил из сопла полосой в десять пикселей, и конус, у которого в точке рождения ширина ноль,
// подменить её не может.
inline framework::graphics::EmitDesc fx_trail(float spd_lo, float spd_hi, float spread, float band,
                                              float life_s, float r, float g, float b, float size) {
    framework::graphics::EmitDesc d;
    d.speed_min = fix32::from_float(spd_lo / FX_TICKS);
    d.speed_max = fix32::from_float(spd_hi / FX_TICKS);
    d.dir_turns = fix32::from_float(0.5);
    d.spread_turns = fix32::from_float(spread);
    d.spawn_half = {fix32{}, fix32::from_float(band)};
    d.damping = fix32::from_float(0.9);
    d.life_ticks = static_cast<uint16_t>(life_s * FX_TICKS + 0.5f);
    d.half_start = fix32::from_float(size / 2.0);
    d.half_end = fix32::from_float(size * 0.4 / 2.0);
    d.rgba_start = fx_rgba(r, g, b, 1.0f);
    d.rgba_end = fx_rgba(r, g, b, 0.0f);
    d.region = RID_Star;
    d.material = MAT_Glow;
    return d;
}

inline const framework::graphics::EmitDesc* fx_table() {
    static framework::graphics::EmitDesc d[FXD_Count];
    static bool built = false;
    if (!built) {
        d[FXD_Fire] = fx_burst(120, 0.65f, 0.92f, 1.0f, 7);
        d[FXD_EnemyDie] = fx_burst(250, 1.0f, 0.62f, 0.22f, 12);
        d[FXD_BossHit] = fx_burst(170, 1.0f, 0.5f, 0.92f, 8);
        d[FXD_BossDieCore] = fx_burst(340, 1.0f, 0.72f, 0.32f, 18);
        d[FXD_BossDieFlash] = fx_burst(140, 1.0f, 1.0f, 0.9f, 22);
        d[FXD_PlayerHit] = fx_burst(220, 1.0f, 0.32f, 0.32f, 11);
        d[FXD_ShipTrail] = fx_trail(90, 150, 0.0133f, 5, 0.34f, 0.55f, 0.85f, 1.0f, 9);
        d[FXD_BulletTrail] = fx_trail(60, 60, 0, 0, 0.16f, 0.6f, 0.95f, 1.0f, 7);
        built = true;
    }
    return d;
}

} // namespace game
