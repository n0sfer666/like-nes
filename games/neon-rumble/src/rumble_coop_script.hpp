#pragma once
#include <array>
#include <cstdint>

#include "input_engine.hpp"
#include "rumble_brawl.hpp"
#include "rumble_wave_script.hpp"

namespace rumble {

// Нажатие (`down`) или отпускание клавиши клавиатуры на тике t — тот же путь, что у окна.
void post_key(input::InputEngine& engine, uint16_t code, bool down, uint32_t t);

// Прогон --wave --coop: оба места играют скрипт волны. P2 входит нампадом на тике 2, проходит с P1
// обе арены и ленту, а когда вид уже в platform-секции, выходит своей клавишей LEAVE и входит снова
// — дорогой лобби, как в окне. P1 с ленты идёт дальше вправо: вход P2 ложится в секцию, а не на улицу.
struct CoopScript {
    std::array<WaveScript, framework::brawl::SEATS> bots{WaveScript{PLAYER, 1960}, WaveScript{PLAYER + 1, 0}};

    void keys(input::InputEngine& engine, uint32_t t) const;
    // Пишет команду только сидящему месту: место без игрока остаётся без `present`.
    void next(Brawl& brawl, uint32_t t);
};

} // namespace rumble
