#pragma once
#include "particles.hpp"

// Таблица описаний и свёртка состояния — общие для трёх целей шага C. Отдельным заголовком по тому
// же основанию, что и `framework_graphics_machine_scene.hpp`: гейт 3 сравнивает ДВА прогона, и
// сцена, написанная в каждой цели заново, сравнивала бы разные сцены, ничего об этом не сказав.
namespace scene {

using ::fix32;
using framework::Vec2;
using framework::graphics::EmitDesc;
using framework::graphics::ParticleStore;

constexpr uint16_t DESCS = 3;

// Три вида: искры (узкий конус вправо, тяжёлые), дым (широкий конус вверх, лёгкий, гаснет),
// невидимый (регион 0 — «не рисовать»). Третий несёт отдельное утверждение, а не полноту таблицы.
inline const EmitDesc* table() {
    static EmitDesc d[DESCS];
    static bool built = false;
    if (!built) {
        d[0].speed_min = fix32::from_float(0.5);
        d[0].speed_max = fix32::from_float(2.0);
        d[0].spread_turns = fix32::from_float(0.05);
        d[0].gravity = {fix32{}, fix32::from_float(0.25)};
        d[0].damping = fix32::from_float(0.94);
        d[0].half_start = fix32::from_float(1.5);
        d[0].half_end = fix32::from_float(0.25);
        d[0].rgba_start = 0xffe08040u;
        d[0].rgba_end = 0x200000ffu;
        d[0].life_ticks = 24;
        d[0].region = 5;
        d[0].material = 1;
        d[0].layer = 2;

        d[1].speed_min = fix32::from_float(0.25);
        d[1].speed_max = fix32::from_float(0.75);
        d[1].dir_turns = fix32::from_float(0.75);
        d[1].spread_turns = fix32::from_float(0.2);
        d[1].damping = fix32::from_float(0.99);
        d[1].half_start = fix32::from_float(0.5);
        d[1].half_end = fix32::from_float(3.0);
        d[1].rgba_start = 0xc0c0c0ffu;
        d[1].rgba_end = 0x40404000u;
        d[1].rate_per_tick = fix32::from_float(2.5);
        d[1].life_ticks = 40;
        d[1].region = 9;
        d[1].material = 1;
        d[1].layer = 3;

        d[2].speed_min = fix32::from_float(1.0);
        d[2].speed_max = fix32::from_float(1.0);
        d[2].life_ticks = 8;
        d[2].region = 0;
        built = true;
    }
    return d;
}

inline void mix(uint64_t& h, int64_t v) {
    for (int32_t i = 0; i < 8; ++i) {
        h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xff);
        h *= 0x100000001b3ull;
    }
}

// Свёртка берёт и ЧАСТИЦЫ, и поток, и потери: два прогона, совпавшие частицами при разошедшемся
// потоке, разойдутся на первой же следующей подаче, и хеш обязан сказать об этом сразу.
inline uint64_t fold(const ParticleStore& e) {
    uint64_t h = 0xcbf29ce484222325ull;
    mix(h, e.count());
    mix(h, e.dropped());
    mix(h, e.stream());
    for (uint32_t i = 0; i < e.count(); ++i) {
        const framework::graphics::Particle& p = e.at(i);
        mix(h, p.pos.x.raw);
        mix(h, p.pos.y.raw);
        mix(h, p.vel.x.raw);
        mix(h, p.vel.y.raw);
        mix(h, p.age.raw);
        mix(h, p.desc);
    }
    return h;
}

// Сценарий подачи: разовые вспышки на разных тиках плюс непрерывный источник. Числа взяты
// взаимно простыми, чтобы вспышки не совпадали с началом такта непрерывного источника.
inline void feed(ParticleStore& e, uint32_t t) {
    const Vec2 at{fix32::from_int(static_cast<int32_t>(t % 13)), fix32::from_int(4)};
    if (t % 7 == 0) e.burst(0, at, 3);
    if (t % 5 == 0) e.burst(2, at, 2);
    e.emit(1, {fix32::from_int(-6), fix32{}}, fix32::from_int(1));
}

} // namespace scene
