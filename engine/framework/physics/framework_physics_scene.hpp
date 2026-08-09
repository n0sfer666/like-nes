#pragma once
#include <vector>

#include "world.hpp"

// Сцена-фикстура гейтов 1, 2 и 6 спеки #15. Общая для трёх тестов, потому что гейт 2 сверяет
// перетасованный порядок создания с обычным, а сверять можно только одну и ту же сцену: две
// «одинаковые» сцены, набранные в двух файлах, расходятся первой же правкой в одном из них.
//
// Состав подобран так, чтобы каждый механизм участвовал в результате: статические коробки дают
// контакт грань-об-грань и стенки, падающие круги — точку об грань и точку об точку, упругость
// ненулевая у кругов и нулевая у ящиков, трение разное. Сцена, где работает только гравитация, дала
// бы golden, устойчивый к поломке решателя.
//
// Вертикаль 2 добавила сюда то, чего механизмы вертикали 1 не умели вовсе и на чём golden обязан
// разойтись при поломке: НАКЛОННЫЕ плиты (поворот статики — единственное, что отличает разделяющую
// ось от сравнения AABB), капсулы и многоугольники (два оставшихся пути узкой фазы), стартовый угол
// и стартовое вращение у части тел (без них момент инерции не входит в результат ни разу).
//
// Наклонные сделаны СКОЛЬЗКИМИ намеренно: при трении 0.6 тело на 15 градусах стоит, и наклон
// проверял бы только то, что контакт нашёлся. Сползающее тело проверяет заодно знак касательного
// импульса и конус — то есть ровно то, что вертикаль 2 переписала.
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
    constexpr Material slick{fix32{}, fix32::from_float(0.1)};
    constexpr fix32 SLOPE = turns_from_degrees(15);
    constexpr fix32 SPIN = fix32::from_float(0.75);
    constexpr fix32 ROLL = fix32::from_float(0.5);

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

    // Наклонные плиты — по одной у каждой стены, спуском к центру. Только они вводят в сцену
    // повёрнутую статику: пол и стены осевые, и без наклона разделяющая ось ни разу не сравнила бы
    // проекции на ось, которой нет среди координатных.
    for (int i = 0; i < 2; ++i) {
        BodyDesc ramp;
        ramp.key = 4 + static_cast<uint32_t>(i);
        ramp.type = BodyType::Static;
        ramp.shape = box(fix32::from_int(72), fix32::from_int(6));
        ramp.position = {fix32::from_int(i == 0 ? -170 : 170), fix32::from_int(40)};
        ramp.angle = i == 0 ? SLOPE : -SLOPE;
        ramp.material = slick;
        out.push_back(ramp);
    }

    // Ящики стартуют с горизонтальной скоростью, чтобы в результат попало трение: без сдвига
    // касательный импульс всегда нулевой, и сломанное трение прошло бы golden незамеченным. У
    // одного задан стартовый угол, у другого — стартовое вращение: момент инерции входит в
    // результат только через них.
    for (int i = 0; i < 5; ++i) {
        BodyDesc b;
        b.key = 10 + static_cast<uint32_t>(i);
        b.shape = box(fix32::from_int(16), fix32::from_int(16));
        b.position = {fix32::from_int(-64 + i * 32), fix32::from_int(-40)};
        b.velocity = {fix32::from_int(i % 2 == 0 ? 40 : -40), fix32{}};
        b.mass = fix32::from_int(4);
        b.material = rough;
        if (i == 1) b.angle = turns_from_degrees(30);
        if (i == 3) b.angular_velocity = SPIN;
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

    // Капсулы и многоугольники — два оставшихся пути узкой фазы. Каждой формы по две: одна
    // сбрасывается на наклонную, вторая падает в общую кучу. Разделение не для красоты — на
    // наклонной опорной оказывается грань ПЛИТЫ, в куче опорной бывает грань самой формы, а это
    // разные ветки выбора опорной грани и разное отсечение; сцена с одним из случаев подписалась бы
    // под сломанной второй веткой.
    for (int i = 0; i < 2; ++i) {
        BodyDesc cap;
        cap.key = 30 + static_cast<uint32_t>(i);
        cap.shape = capsule({fix32::from_int(-10), fix32{}}, {fix32::from_int(10), fix32{}},
                            fix32::from_int(6));
        cap.position = {fix32::from_int(i == 0 ? -220 : -20), fix32::from_int(i == 0 ? 0 : -200)};
        cap.angular_velocity = i == 0 ? ROLL : -ROLL;
        cap.mass = fix32::from_int(3);
        cap.material = i == 0 ? slick : rough;
        out.push_back(cap);
    }

    // Треугольник, а не ещё один четырёхугольник: у него нет пары параллельных граней, поэтому
    // опорная и падающая грани не совпадают по направлению почти никогда — отсечение работает не
    // на вырожденном случае, где обе точки и так лежат внутри.
    const Vec2 tri[3] = {{fix32::from_int(-14), fix32::from_int(10)},
                         {fix32::from_int(14), fix32::from_int(10)},
                         {fix32{}, fix32::from_int(-14)}};
    for (int i = 0; i < 2; ++i) {
        BodyDesc poly;
        poly.key = 40 + static_cast<uint32_t>(i);
        poly.shape = polygon(tri, 3);
        poly.position = {fix32::from_int(i == 0 ? 220 : 30), fix32::from_int(i == 0 ? 0 : -220)};
        poly.angle = i == 0 ? fix32{} : turns_from_degrees(40);
        poly.mass = fix32::from_int(3);
        poly.material = i == 0 ? slick : rough;
        out.push_back(poly);
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
