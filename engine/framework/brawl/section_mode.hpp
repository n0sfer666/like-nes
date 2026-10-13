#pragma once
#include "fixed.hpp"
#include "scene_objects.hpp"

namespace framework::brawl {

// Режим секции по `x`: первая секция с `x0 ≤ x < x1`, вне секций — belt. Секции идут вдоль улицы, и
// `y` их не делит: в platform тело прыгает выше прямоугольника, и переход по `y` выкидывал бы его
// из секции в воздухе.
const scene::Section* section_at(const scene::LevelObjects& level, fix32 x);
scene::SectionMode mode_at(const scene::LevelObjects& level, fix32 x);

// На единицу `raw` левее `v`: последняя точка перед секцией, начинающейся в `v`, — секции полуоткрыты.
fix32 before(fix32 v);

// Ближайший к `x` край, за которым режим остаётся `keep`, — туда зажимается тело с отказанным
// переходом. Правый край секции не её (`x1` исключён), левый — на единицу `raw` левее `x0`.
fix32 refusal_x(const scene::LevelObjects& level, fix32 x, scene::SectionMode keep);

} // namespace framework::brawl
