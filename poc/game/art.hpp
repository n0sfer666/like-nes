#pragma once
#include <cstdint>
#include <vector>

namespace game {

struct Region { float u0, v0, u1, v1; };

struct Atlas {
    std::vector<uint8_t> px;   // RGBA (процедурный путь — mobile-шеллы, fallback)
    std::vector<uint8_t> bc7;  // baked BC7 (шов assetc→бандл); непусто → GPU грузит его
    uint32_t w = 0, h = 0;
    Region ship;
    Region star;
    Region bullet;      // S7: снаряд игрока
    Region enemy;       // S7: враг
    Region digit[10];   // S7: битмап-шрифт 0–9 для HUD (счёт/жизни)
};

// Раскладка atlas'а (UV-регионы + w/h) — метаданные, общие для процедурного и бейкнутого пути.
void set_regions(Atlas& atlas);

// Процедурный atlas (RGBA в коде): mobile-шеллы + fallback, если бандл недоступен.
Atlas build_atlas();

// Шов assetc→билд (спека #8): грузит baked atlas из game.bundle (UASTC→BC7 транскод).
// Пустой bc7 в результате => бандл не открылся (вызывающий делает fallback на build_atlas).
Atlas load_baked_atlas(const char* bundle_path);

// Desktop-хелпер: резолвит game.bundle рядом с exe → baked, иначе процедурный fallback.
// device_supports_bc=false (адаптер без TextureCompressionBC) → сразу процедурный RGBA
// (BC7-текстуру создавать нельзя). Печатает выбранный путь. Единая точка для live/demo.
Atlas load_game_atlas(bool device_supports_bc);

} // namespace game
