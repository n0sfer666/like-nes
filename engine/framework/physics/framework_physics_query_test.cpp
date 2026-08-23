#include <cstdio>

#include "platform_args.hpp"
#include "query.hpp"

// Гейт 4 спеки #15, свипы: raycast и shapecast против таблицы ЭТАЛОННЫХ ответов, посчитанных
// руками из раскладки, а не снятых с прогона. Сцена везде одна и та же и нарочно осевая: коробка
// 16x16 в (50, 0) занимает x от 42 до 58 и y от -8 до 8, поэтому каждое число ниже — арифметика на
// бумаге, и утверждение проверяет движок, а не собственную копию движка.
//
// Свип останавливается за `CONTACT_SLOP` до касания, поэтому доли сверяются с допуском: точное
// равенство пинило бы величину зазора остановки, то есть деталь реализации продвижения.
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

fix32 frac(double v) {
    return fix32::from_float(v);
}


bool close(fix32 got, fix32 want, double eps) {
    return abs_fix(got - want) < fix32::from_float(eps);
}

BodyDesc wall(uint32_t key, fix32 x) {
    BodyDesc d;
    d.key = key;
    d.type = BodyType::Static;
    d.shape = box(fix32::from_int(8), fix32::from_int(8));
    d.position = {x, fix32{}};
    return d;
}

void test_raycast() {
    World w(8);
    w.add(wall(1, fix32::from_int(50)));

    QueryFilter f;
    RayHit hit;

    // Лоб в лоб: левая грань на 42, путь 100 — доля 0.42, нормаль наружу из препятствия.
    check(raycast(w, {fix32{}, fix32{}}, {fix32::from_int(100), fix32{}}, f, hit),
          "a ray straight at the wall hits");
    check(close(hit.fraction, frac(0.42), 0.01), "and reports fraction 42/100");
    check(hit.normal == Vec2{fix32::from_int(-1), fix32{}}, "with the normal out of the wall");
    check(close(hit.point.x, fix32::from_int(42), 0.1), "and the point on the struck face");
    check(hit.key == 1, "and names the body by key");

    // КАСАТЕЛЬНЫЙ случай: луч идёт ровно по y = 8, то есть по прямой, содержащей верхнюю грань. Это
    // худший вход для продвижения — сближение вдоль направления разделения здесь мало, — и ответ
    // обязан остаться тем же, а не «сошлось за 24 итерации куда попало».
    check(raycast(w, {fix32{}, fix32::from_int(8)}, {fix32::from_int(100), fix32{}}, f, hit),
          "a ray grazing the top face still hits the corner");
    check(close(hit.fraction, frac(0.42), 0.01), "at the same fraction as the head-on ray");

    // Негативный двойник касательного: на два юнита выше — мимо. Пара доказывает, что предыдущее
    // утверждение проверяет геометрию, а не то, что свип отвечает «да» на всё.
    check(!raycast(w, {fix32{}, fix32::from_int(10)}, {fix32::from_int(100), fix32{}}, f, hit),
          "two units higher the ray passes over the wall");

    // Длина пути — часть запроса, а не подсказка: луч, кончающийся до стены, не попадает.
    check(!raycast(w, {fix32{}, fix32{}}, {fix32::from_int(30), fix32{}}, f, hit),
          "a ray that ends before the wall does not reach it");
    check(raycast(w, {fix32{}, fix32{}}, {fix32::from_int(50), fix32{}}, f, hit),
          "and the same direction reaching past it does");
    check(close(hit.fraction, frac(0.84), 0.01), "reporting 42/50");

    // Луч ИЗНУТРИ формы: доля ноль, а не «не попал». Ответ «свободно» отсюда означал бы персонажа,
    // стреляющего сквозь стену, в которой стоит.
    check(raycast(w, {fix32::from_int(50), fix32{}}, {fix32::from_int(100), fix32{}}, f, hit),
          "a ray starting inside the wall reports a hit");
    check(hit.fraction.raw == 0, "at fraction zero");

    // Вырожденный запрос — нулевой путь вне формы. Не попадание и не бесконечный перебор.
    check(!raycast(w, {fix32{}, fix32{}}, {fix32{}, fix32{}}, f, hit),
          "a zero-length ray outside everything hits nothing");
}

void test_nearest_and_filter() {
    World w(8);
    w.add(wall(1, fix32::from_int(50)));
    w.add(wall(2, fix32::from_int(100)));
    // Слой дальней стены — второй бит, ближней — первый: запрос сможет спросить именно дальнюю.
    w.mutate(BodyId{1}).layer = 2u;

    const Vec2 origin{fix32{}, fix32{}};
    const Vec2 far_travel{fix32::from_int(200), fix32{}};
    RayHit hit;

    QueryFilter all;
    check(raycast(w, origin, far_travel, all, hit), "a ray across both walls hits");
    check(hit.key == 1, "and returns the NEAREST of the two, not the first found");
    check(close(hit.fraction, frac(0.21), 0.01), "at 42/200");

    // Тот же луч, мимо ближней стены по слою: обязан доехать до дальней. Пара с предыдущим — это и
    // есть доказательство фильтра: геометрия не менялась, изменилась только маска.
    QueryFilter second;
    second.mask = 2u;
    check(raycast(w, origin, far_travel, second, hit), "masking out the near wall still hits");
    check(hit.key == 2, "and now reports the far one");
    check(close(hit.fraction, frac(0.46), 0.01), "at 92/200");

    // Маска, не совпадающая ни с одним слоем, — законный запрос «ни с кем».
    QueryFilter none;
    none.mask = 4u;
    check(!raycast(w, origin, far_travel, none, hit), "a mask matching no layer hits nothing");
}

void test_trigger_visibility() {
    World w(8);
    w.add(wall(1, fix32::from_int(50)));
    w.add(wall(2, fix32::from_int(100)));
    w.mutate(BodyId{0}).trigger = true;

    const Vec2 origin{fix32{}, fix32{}};
    const Vec2 travel{fix32::from_int(200), fix32{}};
    RayHit hit;

    QueryFilter f;
    check(raycast(w, origin, travel, f, hit), "a ray through a trigger zone hits");
    check(hit.key == 2, "and by default sees past it to the solid wall");

    f.include_triggers = true;
    check(raycast(w, origin, travel, f, hit), "asking for triggers hits too");
    check(hit.key == 1, "and now stops at the zone");
}

void test_shapecast() {
    World w(8);
    w.add(wall(1, fix32::from_int(50)));

    QueryFilter f;
    RayHit hit;

    // Круг радиуса 4 касается грани на 42, когда его центр на 38: доля 0.38, а не 0.42. Отличие от
    // луча ровно на радиус — то, что свип формы обязан учитывать и чего луч не знает.
    check(shapecast(w, circle(fix32::from_int(4)), {fix32{}, fix32{}}, fix32{},
                    {fix32::from_int(100), fix32{}}, f, hit),
          "a swept circle hits the wall");
    check(close(hit.fraction, frac(0.38), 0.01), "at 38/100, a radius short of the ray");
    check(hit.normal == Vec2{fix32::from_int(-1), fix32{}}, "with the same outward normal");

    // Тот же круг, поднятый так, что мимо проходит именно ОН, а луч из его центра прошёл бы тоже
    // мимо: 14 > 8 + 4. Негативный двойник, доказывающий, что радиус не растёт бесконтрольно.
    check(!shapecast(w, circle(fix32::from_int(4)), {fix32{}, fix32::from_int(14)}, fix32{},
                     {fix32::from_int(100), fix32{}}, f, hit),
          "raised past the sum of extents the circle misses");

    // А на 11 — задевает: 11 < 8 + 4. Пара утверждений вместе пинит границу, а не сторону от неё.
    check(shapecast(w, circle(fix32::from_int(4)), {fix32{}, fix32::from_int(11)}, fix32{},
                    {fix32::from_int(100), fix32{}}, f, hit),
          "and just inside that sum it grazes the corner");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics query gate\n");
    test_raycast();
    test_nearest_and_filter();
    test_trigger_visibility();
    test_shapecast();
    std::printf("framework-physics-query: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
