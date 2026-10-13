#pragma once
#include <array>
#include <cstdint>
#include <span>

#include "brawl_step.hpp"
#include "rope_swing.hpp"
#include "scene_objects.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

// Тросы уровня — по состоянию на `rope`, в порядке тросов уровня, первые `count` записей. Копируются:
// снапшот — копия без аллокации, потолок `ROPES_MAX` держит загрузчик.
struct StageRopes {
    explicit StageRopes(const scene::LevelObjects& level);
    std::array<RopeState, scene::ROPES_MAX> at{};
    uint32_t count = 0;

    std::span<RopeState> live() { return {at.data(), count}; }
    std::span<const RopeState> live() const { return {at.data(), count}; }
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
