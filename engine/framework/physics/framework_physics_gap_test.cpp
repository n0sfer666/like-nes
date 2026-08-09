#include <cstdio>

#include "body.hpp"
#include "narrowphase.hpp"
#include "platform_args.hpp"

// Перекрытие, живущее ТОЛЬКО в скруглениях (`gap.cpp`). У ядра-отрезка обе нормали перпендикулярны
// ему самому, оси ВДОЛЬ отрезка у разделяющей оси нет вовсе — и пара коллинеарных капсул проходила
// её как пересекающаяся, после чего отсечение боковыми плоскостями выбрасывало все точки до одной и
// узкая фаза отвечала «контакта нет». Наблюдаемо это была капсула, пролетающая сквозь статическую
// капсулу насквозь.
//
// Отдельной целью от двух других путей узкой фазы намеренно: имя упавшей цели в логе CI обязано
// называть сломанный путь.
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

void test_core_gap() {
    BodyDesc a;
    a.key = 1;
    a.shape = capsule({fix32::from_int(-10), fix32{}}, {fix32::from_int(10), fix32{}},
                      fix32::from_int(4));
    BodyDesc b = a;
    b.key = 2;
    // Ядра лежат на ОДНОЙ прямой и разнесены на полюнита при сумме радиусов в восемь: формы
    // пересекаются заведомо, а ни одна нормаль грани этого не выражает.
    b.position = {fix32::from_float(20.5), fix32{}};

    Manifold m;
    check(collide(make_body(a), make_body(b), m), "collinear capsules overlapping by radius collide");
    check(m.count == 1, "a rounding-only overlap gives a single point");
    check(m.normal == Vec2{fix32::from_int(1), fix32{}}, "core-gap normal points a->b");
    check(abs_fix(m.points[0].penetration - fix32::from_float(7.5)) < fix32::from_float(0.05),
          "core-gap penetration = 7.5");

    // Коллинеарная пара, разведённая дальше суммы радиусов, до пути зазора НЕ ДОХОДИТ: её отбивает
    // ось вдоль отрезка (`segments_apart` в узкой фазе). Утверждение здесь про неё и названо так же —
    // приписать этот контроль отсечке пути зазора значило бы получить гейт, который останется
    // зелёным, если ту отсечку удалить.
    b.position = {fix32::from_int(29), fix32{}};
    check(!collide(make_body(a), make_body(b), m), "the segment axis rejects collinear capsules apart");

    // А вот НАСТОЯЩИЙ негативный контроль пути зазора: ядра параллельны и сдвинуты по обеим осям
    // так, что ось вдоль отрезка даёт зазор ровно 8 (не больше суммы радиусов, значит не отбивает),
    // обе опорные грани дают неположительный зазор (значит SAT идёт дальше), а боковые плоскости
    // выбрасывают отсечённый отрезок целиком — то есть решение принимает именно перебор вершин.
    // Истинное расстояние между ядрами 8.54 > 8, и ответ обязан быть «контакта нет»: без собственной
    // отсечки путь зазора отвечал бы «контакт» на разошедшейся паре.
    b.position = {fix32::from_int(28), fix32::from_int(3)};
    check(!collide(make_body(a), make_body(b), m), "the core-gap path rejects a pair 8.54 apart");

    // Позитивный двойник того же пути и той же геометрии: сдвиг на два юнита ближе — расстояние
    // 6.7 < 8, — и он обязан дать контакт. Пара утверждений вместе доказывает, что отсечка стоит на
    // расстоянии, а не отвечает одинаково всем.
    b.position = {fix32::from_int(26), fix32::from_int(3)};
    check(collide(make_body(a), make_body(b), m), "the core-gap path accepts the same pair 6.7 apart");
    check(m.count == 1, "the core-gap path gives a single point");
    check(abs_fix(m.points[0].penetration - fix32::from_float(1.29)) < fix32::from_float(0.05),
          "core-gap penetration = 8 - 6.71");

    // Те же ядра, разнесённые ПОПЕРЁК: ось у SAT здесь есть, и путь обязан остаться прежним — две
    // точки грани об грань, а не одна. Иначе запасной путь молча съел бы и нормальную геометрию.
    b.position = {fix32{}, fix32::from_int(7)};
    check(collide(make_body(a), make_body(b), m), "parallel capsules collide");
    check(m.count == 2, "parallel capsules keep the two-point face manifold");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics core-gap gate\n");
    test_core_gap();
    std::printf("framework-physics-gap: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
