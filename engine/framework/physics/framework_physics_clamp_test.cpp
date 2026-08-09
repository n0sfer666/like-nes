#include <cstdio>

#include "body.hpp"
#include "platform_args.hpp"

// Клампы конструктора тела: что `make_body` делает с входом, который движок объявил вне диапазона.
//
// Отдельной целью от прогонов на границах (`framework_physics_range_test`), потому что вопрос
// другой. Там — «доезжает ли решатель до правильного результата у самого края заявленного», здесь —
// «остаётся ли вход внутри заявленного вообще». Ни один из этих клампов не «валидация»: каждый есть
// условие того, что арифметика ниже не насыщается, а насыщенная величина не «неточна» — она
// произвольна, и решатель считает импульс из произвольного числа.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::physics;

void test_clamps() {
    BodyDesc d;
    d.material = {fix32::from_int(5), fix32::from_int(-3)};
    d.shape = box(fix32::from_int(30000), fix32::from_int(30000));
    d.linear_damping = fix32::from_int(1000);
    d.angular_damping = fix32::from_int(-1);
    const Body b = make_body(d);
    // Упругость выше единицы разгоняла бы тело от каждого удара; отрицательное трение выворачивает
    // границы конуса Кулона и превращает предел в постоянный импульс из ниоткуда.
    check(b.material.restitution == MAX_RESTITUTION, "restitution above one is clamped");
    check(b.material.friction == fix32{}, "negative friction is clamped to zero");
    // Демпфирование выше 1/dt развернуло бы скорость вместо того, чтобы её гасить, отрицательное —
    // разгоняло бы тело в пустоте.
    check(b.linear_damping == MAX_DAMPING, "damping above the ceiling is clamped");
    check(b.angular_damping == fix32{}, "negative damping is clamped to zero");

    fix32 widest{};
    for (uint8_t i = 0; i < b.shape.count; ++i) {
        widest = max_fix(widest,
                         max_fix(abs_fix(b.shape.points[i].x), abs_fix(b.shape.points[i].y)));
    }
    check(widest == MAX_SHAPE_HALF, "an oversized vertex is clamped to the ceiling");
}

// Клампы формы и вращения — то, чего у вертикали 1 не было вовсе. Каждый из них не «валидация
// входа», а условие того, что арифметика ниже не насыщается: насыщенная скорость точки контакта не
// «неточна», она произвольна, и решатель считает импульс из произвольного числа.
void test_shape_clamps() {
    Vec2 many[MAX_VERTICES + 8];
    for (uint32_t i = 0; i < MAX_VERTICES + 8u; ++i) {
        const int32_t k = static_cast<int32_t>(i);
        many[i] = {fix32::from_int(k * k), fix32::from_int(k)};
    }
    BodyDesc crowd;
    crowd.shape = polygon(many, MAX_VERTICES + 8u);
    check(make_body(crowd).shape.count <= MAX_VERTICES, "vertex count is capped");

    // Ядро без площади — законный результат оболочки, а не ошибка: три совпавшие вершины схлопнутся
    // в точку. Но радиус ей назначается принудительно, иначе тело перестаёт сталкиваться вовсе.
    const Vec2 same[3] = {{fix32::from_int(7), fix32::from_int(7)},
                          {fix32::from_int(7), fix32::from_int(7)},
                          {fix32::from_int(7), fix32::from_int(7)}};
    BodyDesc degenerate;
    degenerate.shape = polygon(same, 3);
    const Body db = make_body(degenerate);
    check(db.shape.count == 1, "a collapsed hull becomes a point core");
    check(!(db.shape.radius < MIN_SHAPE_EXTENT), "a core without area gets a radius");

    // Невыпуклый вход — не ошибка вызывающего, которую можно задокументировать: SAT на нём даёт
    // нормаль ВНУТРЬ, и тело затягивает в препятствие. Лишняя вершина обязана исчезнуть.
    const Vec2 dented[5] = {{fix32::from_int(-10), fix32::from_int(-10)},
                            {fix32::from_int(10), fix32::from_int(-10)},
                            {fix32::from_int(10), fix32::from_int(10)},
                            {fix32::from_int(-10), fix32::from_int(10)},
                            {fix32{}, fix32{}}};
    BodyDesc concave;
    concave.shape = polygon(dented, 5);
    check(make_body(concave).shape.count == 4, "an interior vertex is dropped by the hull");

    BodyDesc spin;
    spin.shape = box(fix32::from_int(1024), fix32::from_int(1024));
    spin.angular_velocity = MAX_ANG_SPEED;
    const Body sb = make_body(spin);
    check(sb.max_angular < MAX_ANG_SPEED, "a large shape gets a lower ceiling than the global one");
    check(sb.angular_velocity == sb.max_angular, "angular speed is clamped to the body ceiling");
    // Позитивный контроль: потолок проверяется не тем, что он «меньше», а тем, ради чего он введён
    // — скорость точки поверхности обязана остаться внутри арифметического потолка.
    check(!(MAX_SPEED < TAU * sb.angular_velocity * reach(sb.shape)),
          "the surface speed stays inside the arithmetic ceiling");

    // Тот же контроль на САМОЙ КРУПНОЙ законной форме, а не только на удобной средней. Потолок
    // считался через произведение TAU * reach в Q16.16, а оно насыщается уже на 5215 юнитах: у всего,
    // что крупнее, знаменатель переставал расти, потолок выходил одинаковым — и скорость поверхности
    // тем сильнее превышала арифметический предел, чем крупнее форма, ровно наоборот замыслу.
    // Коробка 4096x4096 даёт reach 5792, то есть попадает в зону насыщения, а 1024x1024 (1448) — нет.
    BodyDesc widest;
    widest.shape = box(MAX_SHAPE_HALF, MAX_SHAPE_HALF);
    widest.angular_velocity = MAX_ANG_SPEED;
    const Body wb = make_body(widest);
    check(wb.max_angular < sb.max_angular, "a wider shape gets a strictly lower ceiling");
    check(!(MAX_SPEED < TAU * wb.angular_velocity * reach(wb.shape)),
          "the surface speed holds on the widest legal shape too");

    // Сдвиг ядра к центроиду — ПЕРЕНОС, и он вправе вынести вершину за потолок полуразмера: у иглы
    // с вершиной на 4096 центроид стоит на -1365, и после сдвига дальняя вершина оказывается на 5461.
    // За потолком начинается насыщение AABB, о котором широкая фаза молчит, поэтому приведение
    // прогоняется второй раз — уже после сдвига.
    const Vec2 needle[3] = {{MAX_SHAPE_HALF, fix32{}},
                            {-MAX_SHAPE_HALF, fix32::from_int(-1)},
                            {-MAX_SHAPE_HALF, fix32::from_int(1)}};
    BodyDesc spike;
    spike.shape = polygon(needle, 3);
    const Body nb = make_body(spike);
    fix32 farthest{};
    for (uint8_t i = 0; i < nb.shape.count; ++i) {
        farthest = max_fix(farthest,
                           max_fix(abs_fix(nb.shape.points[i].x), abs_fix(nb.shape.points[i].y)));
    }
    check(!(MAX_SHAPE_HALF < farthest), "the centroid shift cannot push a vertex past the ceiling");

    // Приведение угла к периоду — точное, в обе стороны. Без него тело, провернувшееся на оборот
    // больше, хешировалось бы иначе при том же положении на экране.
    BodyDesc turned;
    turned.angle = fix32::from_int(10) + turns_from_degrees(90);
    check(make_body(turned).angle == turns_from_degrees(90), "a wound-up angle folds into one turn");
    turned.angle = turns_from_degrees(-90);
    check(make_body(turned).angle == turns_from_degrees(270), "a negative angle folds forward");
}

void test_vector_contracts() {
    // Потолок скорости — потолок и тогда, когда длина вектора НАСЫЩАЕТСЯ. Прежняя схема делила на
    // насыщенную длину и возвращала (2048, 2048), то есть модуль 2896 при потолке 2048.
    const Vec2 huge = {fix32::from_raw(INT32_MAX), fix32::from_raw(INT32_MAX)};
    check(!(MAX_SPEED < length(clamp_speed(huge, MAX_SPEED))), "the speed ceiling holds at saturation");

    // Нормаль остаётся единичной на самом коротком ненулевом векторе: деление на округлённую вниз
    // длину давало здесь модуль 1.41, а его ошибка входит в импульс дважды.
    constexpr fix32 TOL = fix32::from_float(1.004);
    Vec2 n;
    normalize({fix32::from_raw(1), fix32::from_raw(1)}, n);
    const fix32 len = length(n);
    check(!(TOL < len) && fix32::from_float(0.996) < len, "the normal stays unit on a raw-unit vector");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics clamp gate\n");
    test_clamps();
    test_shape_clamps();
    test_vector_contracts();
    std::printf("framework-physics-clamp: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
