#pragma once
#include <span>

#include "archetype.hpp"
#include "body_pool.hpp"
#include "brawl_step.hpp"
#include "hit_events.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

struct Shot {
    uint8_t kind = 0;
    DepthBody at{};
    uint16_t life = 0;
};

// Снаряд (спека #25, В8г) — тело вида `Role::Projectile` с `hp` 0: сторона, `facing` и режим —
// владельца на выпуске (режим — по записи владельца в секциях, без неё — belt). Отказ — пустой `EntId`.
// Выпуск на территории чужого режима гибнет на первой его границе по пути от владельца (нет её —
// `refusal_x`) с `life` 1, как упёршийся; границу стережёт только мир с `level` и `platform`.
// `fly_projectile` упирает его в мир или в границу своего режима и ставит `life` 1: бьёт он в этом же
// тике. `retire_projectiles` после ударов только метит — сработавший `life` 0, прочим `--life`;
// тело снимает `drop_spent`, и зовёт его игра в начале тика, ДО команд по слотам пула: внутри
// `step_stage` или после шага он сдвинул бы слоты под уже собранными командами.
EntId spawn_projectile(BodyPool& pool, StageSections* sections, EntId owner, const Shot& shot, const BrawlWorld& w);
void fly_projectile(Body& b, const BrawlWorld& w, bool platform);
void retire_projectiles(BodyPool& pool, std::span<const Archetype> kinds, const HitEvents& events);
void drop_spent(BodyPool& pool, std::span<const Archetype> kinds, StageSections* sections);

} // namespace framework::brawl
