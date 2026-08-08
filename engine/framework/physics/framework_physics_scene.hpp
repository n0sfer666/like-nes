#pragma once
#include <vector>

#include "world.hpp"

// Сцена-фикстура гейтов 1, 2 и 6 спеки #15. Общая для трёх тестов, потому что гейт 2 сверяет
// перетасованный порядок создания с обычным, а сверять можно только одну и ту же сцену: две
// «одинаковые» сцены, набранные в двух файлах, расходятся первой же правкой в одном из них.
//
// Состав подобран так, чтобы каждый механизм вертикали 1 участвовал в результате: статические
// коробки дают контакт коробка-коробка и стенки, падающие круги — круг-коробка и круг-круг,
// упругость ненулевая у кругов и нулевая у ящиков, трение разное. Сцена, где работает только
// гравитация, дала бы golden, устойчивый к поломке решателя.
namespace framework::physics::fixture {

constexpr uint32_t CAPACITY = 32;
constexpr uint32_t STEPS = 180;

inline fix32 step_dt() { return fix32::from_int(1) / fix32::from_int(60); }

inline void describe(std::vector<BodyDesc>& out) {
    out.clear();
    // constexpr, а не const: `from_float` разрешён только в константах на границе, и вычисление
    // на этапе компиляции — единственное, что отличает константу от вызова в горячем коде.
    constexpr Material rough{fix32{}, fix32::from_float(0.6)};
    constexpr Material bouncy{fix32::from_float(0.4), fix32::from_float(0.2)};

    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(256), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = rough;
    out.push_back(floor);

    for (int i = 0; i < 2; ++i) {
        BodyDesc wall;
        wall.key = 2 + static_cast<uint32_t>(i);
        wall.type = BodyType::Static;
        wall.shape = box(fix32::from_int(8), fix32::from_int(128));
        wall.position = {fix32::from_int(i == 0 ? -256 : 256), fix32::from_int(64)};
        wall.material = rough;
        out.push_back(wall);
    }

    // Ящики стартуют с горизонтальной скоростью, чтобы в результат попало трение: без сдвига
    // касательный импульс всегда нулевой, и сломанное трение прошло бы golden незамеченным.
    for (int i = 0; i < 5; ++i) {
        BodyDesc b;
        b.key = 10 + static_cast<uint32_t>(i);
        b.shape = box(fix32::from_int(16), fix32::from_int(16));
        b.position = {fix32::from_int(-64 + i * 32), fix32::from_int(-40)};
        b.velocity = {fix32::from_int(i % 2 == 0 ? 40 : -40), fix32{}};
        b.mass = fix32::from_int(4);
        b.material = rough;
        out.push_back(b);
    }

    for (int i = 0; i < 5; ++i) {
        BodyDesc c;
        c.key = 20 + static_cast<uint32_t>(i);
        c.shape = circle(fix32::from_int(12));
        c.position = {fix32::from_int(-48 + i * 24), fix32::from_int(-140)};
        c.velocity = {fix32::from_int(i - 2) * fix32::from_int(20), fix32{}};
        c.mass = fix32::from_int(2);
        c.material = bouncy;
        out.push_back(c);
    }
}

inline void fill(World& w, const std::vector<BodyDesc>& descs) {
    for (const BodyDesc& d : descs) w.add(d);
}

inline void run(World& w, uint32_t steps) {
    const fix32 dt = step_dt();
    for (uint32_t i = 0; i < steps; ++i) w.step(dt);
}

} // namespace framework::physics::fixture
