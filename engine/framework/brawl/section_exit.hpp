#pragma once
#include "body_pool.hpp"
#include "brawl_step.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

// Занятость точки выхода. Тела друг друга не толкают ни в одном режиме, поэтому «тело занимает
// точку» — пересечение следов: в platform — прямоугольников корпуса, в belt — ширины корпуса по `x`
// и толщины по `z`, как у ударов.
bool platform_taken(const BodyPool& pool, const StageSections& sections, EntId self, Vec2 at, const BrawlWorld& w);
bool belt_taken(const BodyPool& pool, const StageSections& sections, const Body& self, fix32 z, const BrawlWorld& w);

} // namespace framework::brawl
