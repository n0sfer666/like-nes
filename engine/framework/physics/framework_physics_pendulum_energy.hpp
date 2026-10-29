#pragma once
#include <cstdint>

#include "fixtrig.hpp"
#include "pendulum.hpp"

namespace framework::physics::test {

// Теневая энергия полунеявного Эйлера, px²/тик² в Q16.16 без сдвига: `½v² + gL(1 − cos θ) − ½·v·g·sin θ`.
// Её шаг сохраняет точно, а не `½v² + gL(1 − cos θ)`: та колеблется на полшага каждый тик и судила
// бы о росте по колебанию, а не по росту.
inline int64_t pendulum_energy(const Pendulum& p, fix32 length, fix32 g) {
    const fix32 t = pendulum_turns(p.arc, length);
    const int64_t v = p.speed.raw;
    const int64_t kinetic = v * v / 2;
    const int64_t potential = static_cast<int64_t>((g * length).raw) * (fix32::from_int(1) - cos_turns(t)).raw;
    const int64_t shadow = v * (g * sin_turns(t)).raw / 2;
    return kinetic + potential - shadow;
}

// Гейт энергии: пик каждого окна из `WINDOW` тиков не выше пика прошлого, пик последнего полного
// окна ниже энергии начала. Окно, а не тик: энергия на тике колеблется в младших разрядах и на честном шаге.
// Пик первого окна с началом не сравнивается: дуга в угол округляется, и первые тики выходят над
// ним на доли процента (замер — до 0.26 % на 256 px и 5°), а не на рост от шага к шагу.
struct EnergyGate {
    static constexpr uint32_t WINDOW = 500;
    int64_t start;
    int64_t last_peak;
    int64_t peak;
    uint32_t tick = 0;
    bool rose = false;

    explicit EnergyGate(int64_t e) : start(e), last_peak(INT64_MAX), peak(e) {}

    void add(int64_t e) {
        if (++tick % WINDOW == 0) {
            rose = rose || last_peak < peak;
            last_peak = peak;
            peak = e;
        }
        if (peak < e) peak = e;
    }

    bool settles() const { return !rose && last_peak < start; }
};

} // namespace framework::physics::test
