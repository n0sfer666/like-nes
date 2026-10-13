#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"
#include "rumble_fighter_quads.hpp"
#include "rumble_layers.hpp"

namespace rumble {

// Примитивы platform-секций (спека #25, В8) — отладочные заливки без ассетов, под бойцами: лента —
// её прямоугольник, лифт — тело физмира `w × h`, трос — точки от опоры до конца и конец крупнее.
FighterStats draw_stage(FighterQuads& quads, const Brawl& brawl, Layers& layers, LayerStats& st,
                        uint32_t solid_texture);

} // namespace rumble
