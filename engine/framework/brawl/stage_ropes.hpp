#pragma once
#include <span>
#include <vector>

#include "brawl_step.hpp"
#include "rope_swing.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

// Тросы уровня — по состоянию на `rope`, в порядке тросов уровня. Копируются: снапшот — копия.
struct StageRopes {
    explicit StageRopes(const scene::LevelObjects& level);
    std::vector<RopeState> at;
};

bool rides(const StageRopes* ropes, EntId id);

// После физмира, до тел (спека #25, система 4): трос с седоком качает его и носит концом, свободный
// качается сам, затем тела в воздухе с зажатым захватом берут трос — в порядке пула, первый трос
// уровня в досягаемости; отпущенный в этом тике не берёт никакой, иначе соседний трос съел бы
// прыжок. Седока контроллер в этом тике не двигает (`step_bodies`), FSM тикает. Abort с причиной —
// уровень с `rope` без `ropes`, `ropes`, построенные для другого числа тросов, тросы без
// `w.platform`.
void step_ropes(StageRopes* ropes, BodyPool& pool, std::span<const Command> commands, StageSections& sections,
                const BrawlWorld& w);

} // namespace framework::brawl
