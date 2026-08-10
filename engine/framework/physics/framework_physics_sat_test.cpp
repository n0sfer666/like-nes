#include <cstdio>

#include "body.hpp"
#include "narrowphase.hpp"
#include "platform_args.hpp"
#include "shape.hpp"

// Постоянный манифольд отсечения (`sat.cpp`): сколько точек даёт пересечение и почему именно
// столько. Число точек здесь не деталь реализации, а поведение — одна точка не создаёт момента.
//
// Отдельной целью от двух других путей узкой фазы намеренно: имя упавшей цели в логе CI обязано
// называть сломанный путь. Хеш сцены (гейт 1) этого не различает вовсе — раскладка, где грань об
// грань выдаёт одну точку вместо двух, посчитается одинаково на всех трёх платформах и пройдёт
// голден, а ящик на полу при этом медленно завалится на угол.
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

void test_manifold() {
    BodyDesc plate;
    plate.key = 1;
    plate.type = BodyType::Static;
    plate.shape = box(fix32::from_int(50), fix32::from_int(10));
    plate.position = {fix32{}, fix32::from_int(20)};
    const Body ground = make_body(plate);

    BodyDesc crate;
    crate.key = 2;
    crate.shape = box(fix32::from_int(10), fix32::from_int(10));
    crate.position = {fix32{}, fix32::from_int(2)};

    Manifold m;
    check(collide(make_body(crate), ground, SPECULATIVE_MARGIN, m), "box-box overlap detected");
    check(m.count == 2, "face on face gives two points");
    check(m.normal == Vec2{fix32{}, fix32::from_int(1)}, "box-box normal points down to plate");
    check(m.points[0].id != m.points[1].id, "manifold points carry distinct ids");

    // Тот же ящик, повёрнутый на 45 градусов, стоит на плите УГЛОМ — точка одна. Ничего, кроме
    // поворота, не изменилось: это и есть проверка того, что SAT сравнивает проекции на ось формы,
    // а не на координатные оси, как сравнивал бы AABB.
    crate.angle = turns_from_degrees(45);
    crate.position = {fix32{}, fix32::from_int(-3)};
    check(collide(make_body(crate), ground, SPECULATIVE_MARGIN, m), "rotated box still collides");
    check(m.count == 1, "corner on face gives one point");

    // Капсула лежит на плите: её ядро — отрезок, и опорной гранью становится он сам. Без этого
    // пути форма без площади не дала бы SAT ни одной оси для перебора.
    BodyDesc cap;
    cap.key = 3;
    cap.shape = capsule({fix32::from_int(-12), fix32{}}, {fix32::from_int(12), fix32{}},
                        fix32::from_int(4));
    cap.position = {fix32{}, fix32::from_int(7)};
    check(collide(make_body(cap), ground, SPECULATIVE_MARGIN, m), "capsule-box overlap detected");
    check(m.count == 2, "lying capsule gives two points");
    check(m.normal == Vec2{fix32{}, fix32::from_int(1)}, "capsule-box normal points down");

    const Vec2 tri[3] = {{fix32::from_int(-14), fix32::from_int(10)},
                         {fix32::from_int(14), fix32::from_int(10)},
                         {fix32{}, fix32::from_int(-14)}};
    BodyDesc wedge;
    wedge.key = 4;
    wedge.shape = polygon(tri, 3);
    wedge.position = {fix32{}, fix32::from_int(-3)};
    wedge.angle = turns_from_degrees(180);
    check(collide(make_body(wedge), ground, SPECULATIVE_MARGIN, m), "polygon-box overlap detected");
    check(m.count == 1, "apex on face gives one point");
    check(m.normal == Vec2{fix32{}, fix32::from_int(1)}, "polygon-box normal points down");
}

// Глубина, измеренная НЕЗАВИСИМО от узкой фазы: проекции обоих ядер на ось манифольда. Перекрытие
// вдоль нормали и есть проникновение — это определение разделяющей оси, а не совпадение величин.
// Утверждение построено так намеренно: сверять нормаль с прошитой константой значило бы пересказать
// реализацию своими словами, а перекрытие связывает две величины манифольда МЕЖДУ СОБОЙ и ловит
// любую из них, взятую не оттуда.
fix32 overlap_along(const Body& a, const Body& b, Vec2 axis) {
    WorldShape wa;
    WorldShape wb;
    to_world(a.shape, a.position, a.rot, wa);
    to_world(b.shape, b.position, b.rot, wb);
    fix32 a_max = dot(axis, wa.points[0]);
    for (uint8_t i = 1; i < wa.count; ++i) a_max = max_fix(a_max, dot(axis, wa.points[i]));
    fix32 b_min = dot(axis, wb.points[0]);
    for (uint8_t i = 1; i < wb.count; ++i) b_min = min_fix(b_min, dot(axis, wb.points[i]));
    return a_max - b_min + wa.radius + wb.radius;
}

// Ядра пересеклись, а отсечение граней не выразило это НИ ОДНОЙ точкой: отсечённый отрезок
// выбрасывают боковые плоскости целиком. Ответ обязан остаться на опорной оси SAT — у пары с
// площадью перебор нормалей полон, и он уже доказал пересечение.
//
// Раскладка не придумана: перебор 51 млн пар (две формы, 32 угла каждой, сетка сдвигов с шагом
// в четверть юнита) нашёл восемь, и эта — первая из них. Именно поэтому она выглядит произвольной:
// касание идёт по краю, глубина сотые доли юнита. До фикса пара уходила на путь ЗАЗОРА, который ищет
// минимум расстояния между множествами и на пересекающихся не находит ничего: он возвращал нормаль
// от вершины в восемнадцати юнитах — перпендикулярную настоящей — с нулевой глубиной. Тела при этом
// разъезжались поперёк.
//
// Перебор перезапущен со спекулятивным полем, и прежняя раскладка сюда больше не приходит: поле
// принимает точку отсечения по ЗАЗОРУ до 1/16, поэтому пара, у которой все точки отбрасывались за
// сотые доли юнита, теперь выражается отсечением честно. Это не потеря пути, а сужение входа в
// него — и потому раскладка взята новая, а не старая с обходом поля: ветка обязана проверяться на
// том же поле, с которым её зовёт шаг.
void test_reference_axis() {
    const Vec2 tri[3] = {{fix32::from_int(-14), fix32::from_int(9)},
                         {fix32::from_int(11), fix32::from_int(13)},
                         {fix32::from_int(2), fix32::from_int(-16)}};
    BodyDesc a;
    a.key = 1;
    a.shape = polygon(tri, 3);
    a.angle = fix32::from_float(0.15625); // 56.25 градуса, в оборотах — точная двоичная дробь
    BodyDesc b;
    b.key = 2;
    b.shape = polygon(tri, 3);
    b.angle = fix32::from_float(0.53125); // 191.25 градуса
    b.position = {fix32::from_float(19.5), fix32::from_float(-22.5)};

    const Body ba = make_body(a);
    const Body bb = make_body(b);
    Manifold m;
    check(collide(ba, bb, SPECULATIVE_MARGIN, m), "control: cores touching by a sliver still report a contact");
    check(m.count == 1, "an empty clip on met cores gives one point on the reference axis");
    // Утверждение про ПУТЬ, а не только про величины, и на этой раскладке оно единственное с зубами:
    // отключённая ветка (проверено — `if (false && cores_meet)`) роняет пару на путь зазора, и тот
    // здесь возвращает одну точку с положительной глубиной, то есть проходит и счётчик, и сверку с
    // перекрытием. Величины совпали случайно, путь — нет: старший бит идентификатора называет
    // ПРАВИЛО, которым точка посчитана, и подменить его нечем.
    check((m.points[0].id & REF_AXIS_ID_BIT) != 0, "and it comes from the reference-axis path");
    check(m.points[0].penetration.raw > 0, "and that point carries a depth, not a bare flag");
    check(abs_fix(overlap_along(ba, bb, m.normal) - m.points[0].penetration) <
              fix32::from_float(0.01),
          "the reported depth is the overlap along the reported normal");

    // Та же пара на НУЛЕВОМ поле — вход запросов и трассировки (`query.cpp`, `cast.cpp`). У них поля
    // нет вовсе, а развилка та же самая, и ответ обязан совпасть до разряда: перекрытие ядер поле не
    // создаёт и не отменяет — оно лишь расширяет полосу, в которой контакт вообще рассматривают.
    WorldShape wa;
    WorldShape wb;
    to_world(ba.shape, ba.position, ba.rot, wa);
    to_world(bb.shape, bb.position, bb.rot, wb);
    Manifold z;
    check(collide_shapes(wa, ba.position, wb, bb.position, fix32{}, z),
          "the same overlap is reported with no speculative margin at all");
    check(z.count == m.count && z.points[0].id == m.points[0].id &&
              z.points[0].penetration.raw == m.points[0].penetration.raw,
          "and the query path lands on the same point with the same depth");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics manifold gate\n");
    test_manifold();
    test_reference_axis();
    std::printf("framework-physics-sat: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
