#pragma once
#include <array>
#include <cstdint>

#include "ent_id.hpp"

namespace framework::ai {

using core::EntId;

constexpr uint32_t BRAINS_MAX = 8;

// Состояние FSM — индекс в таблицу функций игры, таймер — в тиках. Фреймворк хранит, игра решает.
struct Brain {
    EntId body;
    uint8_t state = 0;
    uint16_t timer = 0;
    EntId target;
};

// Порядок по seq тела — порядок шага: мозги делят жетоны и ГСЧ, и кто спросил первым, тот и взял.
struct Brains {
    std::array<Brain, BRAINS_MAX> at{};
    uint32_t count = 0;
};

bool add_brain(Brains& b, EntId body, uint8_t state);
Brain* find_brain(Brains& b, EntId body);

} // namespace framework::ai
