#pragma once

#include "framework_physics_observed.hpp"
#include "platformer_scene.hpp"

// Наблюдаемое сцены целиком — всё, что игра вправе спросить у уровня между кадрами.
//
// Заведено затем, чтобы утверждение об откате звучало как «сцена НЕОТЛИЧИМА от точки снятия», а не
// как «позиция сошлась». Мир отдаёт свой набор сам (`framework_physics_observed.hpp`), персонаж
// сравнивается поимённо: хеш мира считается по телам и молчит про окна прощения, запомненную опору
// и флаги касания, а после отката игра читает именно их — анимация удара о потолок и звук трения о
// стену берут `hit_ceiling`/`hit_wall`, а не выводят их из обнулившейся скорости.
//
// Различие называется ПОИМЁННО: «сцены разошлись» и «разошлось окно прыжкового буфера» — разные
// строки в логе, и вторая указывает на поле снимка. Отдельным файлом от гейта, потому что шагу C
// тот же словарь нужен для сверки двух ПРОЦЕССОВ, а не двух прогонов в одном.
namespace platformer {

struct Mark {
    ph::observed::Observed world;
    ch::Character hero;
};

inline Mark observe(const Stage& st) {
    Mark m;
    m.world = ph::observed::observe(st.world);
    m.hero = st.hero;
    return m;
}

inline const char* hero_difference(const ch::Character& a, const ch::Character& b) {
    if (a.position.x.raw != b.position.x.raw || a.position.y.raw != b.position.y.raw)
        return "hero position";
    if (a.velocity.x.raw != b.velocity.x.raw || a.velocity.y.raw != b.velocity.y.raw)
        return "hero velocity";
    if (a.state != b.state) return "hero move state";
    if (a.coyote_left != b.coyote_left) return "coyote window";
    if (a.buffer_left != b.buffer_left) return "jump buffer";
    if (a.ladder_regrab_left != b.ladder_regrab_left) return "ladder regrab window";
    if (a.on_ground != b.on_ground) return "on-ground flag";
    if (a.jump_active != b.jump_active) return "jump hold";
    if (a.jump_was_held != b.jump_was_held) return "jump edge memory";
    if (a.hit_ceiling != b.hit_ceiling) return "ceiling touch";
    if (a.hit_wall != b.hit_wall) return "wall touch";
    if (a.support.index != b.support.index) return "remembered support";
    if (a.crushed != b.crushed) return "crushed flag";
    return nullptr;
}

// `nullptr` — сцены неотличимы. Мир первым: расхождение в нём объясняет расхождение персонажа, а
// не наоборот.
inline const char* difference(const Mark& a, const Mark& b) {
    if (const char* d = ph::observed::first_difference(a.world, b.world)) return d;
    return hero_difference(a.hero, b.hero);
}

} // namespace platformer
