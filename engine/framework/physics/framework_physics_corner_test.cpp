#include <cstdio>

#include "cast.hpp"
#include "distance.hpp"
#include "platform_args.hpp"

// Нормаль свипа на ВЫРОЖДЕННОМ касании — когда угол мувера приходит ровно в угол препятствия.
//
// Свидетель расстояния (`core_distance`) на такой паре отвечает произвольно: вершина, легшая в угол,
// уходит в ветку «точка на ядре», где две смежные грани дают одинаковый ноль, и побеждает та, что
// раньше по номеру. Наблюдалось это как нормаль (0, 1) у коробки, въехавшей в стену слева, — то есть
// наружу из грани, которой мувер не касался вовсе. На сетке тайлов совпавшие углы норма, а не
// экзотика: всё стоит на одном шаге.
//
// Сцена осевая и на целых числах, поэтому каждое число ниже — арифметика на бумаге. Доли сверяются с
// допуском: продвижение останавливается за `CONTACT_SLOP` до касания, и точное равенство пинило бы
// величину зазора остановки, то есть деталь реализации.
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

fix32 fx(int v) {
    return fix32::from_int(v);
}

bool close(fix32 got, fix32 want, double eps) {
    return abs_fix(got - want) < fix32::from_float(eps);
}

// Коробка, развёрнутая в мир без поворота. Ядро санируется один раз на вызов: оболочка задаёт порядок
// вершин, и считать его руками в тесте значило бы держать вторую копию `sanitize`.
void place_box(Vec2 center, fix32 half_w, fix32 half_h, WorldShape& out) {
    to_world(sanitize(box(half_w, half_h)), center, rotation(fix32{}), out);
}

void unit_box(Vec2 center, WorldShape& out) {
    place_box(center, fx(8), fx(8), out);
}

// Горизонтальный свип на коробку с СОВПАВШИМИ горизонтальными границами: обе занимают y от 48 до 64,
// и в момент касания совпадают не грани, а именно углы — по два с каждой стороны.
void test_coplanar_faces() {
    WorldShape mover, target;
    unit_box({fx(24), fx(56)}, mover);   // x от 16 до 32
    unit_box({fx(72), fx(56)}, target);  // x от 64 до 80

    CastHit hit;
    check(cast_shape(mover, {fx(24), fx(56)}, {fx(64), fix32{}}, target, {fx(72), fx(56)}, hit),
          "a box swept into a box with coincident top and bottom edges hits");
    check(close(hit.fraction, fix32::from_float(0.5), 0.01), "at 32/64 of the path");
    check(hit.normal == Vec2{fx(-1), fix32{}}, "with the normal out of the STRUCK face, not a tied one");

    // Тот же ход, но границы разведены на 4: ничьей нет вовсе, и ответ обязан совпасть. Пара
    // доказывает, что верхнее утверждение проверяет разрешение ничьей, а не то, что свип вообще
    // умеет отвечать (-1, 0).
    unit_box({fx(24), fx(52)}, mover);
    check(cast_shape(mover, {fx(24), fx(52)}, {fx(64), fix32{}}, target, {fx(72), fx(56)}, hit),
          "the same sweep with the edges 4 units apart hits too");
    check(hit.normal == Vec2{fx(-1), fix32{}}, "with the same outward normal");

    // Негативный двойник: цель опущена так, что по вертикали остаётся зазор в юнит — больше
    // `CONTACT_SLOP`. Горизонтальный путь по вертикали не сближает, и касания нет.
    unit_box({fx(72), fx(39)}, target);  // y от 31 до 47, мувер — от 44 до 60
    unit_box({fx(24), fx(56)}, mover);   // y от 48 до 64
    check(!cast_shape(mover, {fx(24), fx(56)}, {fx(64), fix32{}}, target, {fx(72), fx(39)}, hit),
          "and a unit of vertical clearance is a miss, not a corner touch");
}

// Та же вырожденность по ДРУГОЙ оси: совпадают вертикальные границы, движение сверху вниз. Ответ
// обязан быть (0, -1), а не «всегда (-1, 0)» — без этого случая гейт пинил бы константу.
void test_coplanar_faces_vertical() {
    WorldShape mover, target;
    unit_box({fx(72), fix32{}}, mover);   // y от -8 до 8, x от 64 до 80
    unit_box({fx(72), fx(56)}, target);   // y от 48 до 64, x тот же

    CastHit hit;
    check(cast_shape(mover, {fx(72), fix32{}}, {fix32{}, fx(64)}, target, {fx(72), fx(56)}, hit),
          "a box dropped onto a box with coincident side edges hits");
    check(close(hit.fraction, fix32::from_float(0.625), 0.01), "at 40/64 of the path");
    check(hit.normal == Vec2{fix32{}, fx(-1)}, "with the normal out of the top face (+Y is down)");
}

// ПОЗИТИВНЫЙ КОНТРОЛЬ. Утверждения выше зелены и на прежней реализации, если вырожденности на этой
// паре нет вовсе, — поэтому вырожденность утверждается отдельно и на той же геометрии: коробки
// поставлены ровно в касание, и спрашивается тот самый свидетель, у которого свип раньше брал ответ.
//
// Гейт покраснеет и в тот день, когда перебор вершин на угловой ничьей начнёт отвечать верно. Это
// намеренно: посылка контроля исчезла, и решение «нормаль берётся у узкой фазы» обязано быть
// перечитано, а не пережить свою причину молча.
void test_witness_alone_is_ambiguous() {
    WorldShape a, b;
    unit_box({fx(56), fx(56)}, a);  // x от 48 до 64
    unit_box({fx(72), fx(56)}, b);  // x от 64 до 80 — грани сомкнуты, углы совпали

    const CoreDistance d = core_distance(a, b);
    std::printf("  witness on the touching pair: dist=%d dir=(%d, %d) raw\n", d.dist.raw, d.dir.x.raw,
                d.dir.y.raw);
    check(d.dist.raw == 0, "the touching pair really is at zero core distance");
    check(!(-d.dir == Vec2{fx(-1), fix32{}}),
          "and the vertex walk alone names a face the mover never touched");
}

// КРАЙ ПОЛА ПО ДИАГОНАЛИ — та раскладка, на которой дефект сдвинул голден траектории персонажа,
// поэтому она пинится сценой, а не одним лишь описанием. Персонаж уперся во внутренний угол между
// стеной и полом, и после разбора касания со стеной остаток пути идёт вниз-влево: нижний-левый угол
// корпуса летит прямо в верхний-левый угол пола. Перебор вершин сцепляет именно эту пару углов —
// она и правда ближайшая ПО ЯДРАМ — и отдаёт диагональ (0.707, -0.707). Но продвижение
// останавливается ДО касания, корпус в этот момент ещё стоит над полом целиком, перекрываясь с его
// верхней гранью во всю свою ширину, и верный ответ — (0, -1). Отход на зазор идёт ВДОЛЬ нормали,
// поэтому диагональ толкала персонажа вбок и отлепляла его от стены.
//
// Длина пути тут НЕСУЩАЯ, а не декоративная: на длине, кратной степени двойки, продвижение попадает
// в угол ТОЧНО, ничья решается вырожденной веткой «точка на ядре» и случайно даёт верную грань. Три
// юнита остатка не дают, и свидетель остаётся при диагонали — на прежней реализации гейт красный
// именно на этой длине.
void test_floor_edge_diagonal() {
    const fix32 skin = fix32::from_float(0.125);
    const Vec2 at = {fx(-300) + fx(8) + skin, -fx(16) - skin};  // левая грань и низ в зазоре 1/8
    const Vec2 down_left = {fx(-3), fx(3)};
    WorldShape mover, floor;
    place_box(at, fx(8), fx(16), mover);
    place_box({fix32{}, fx(100)}, fx(300), fx(100), floor);  // верх ровно на y = 0, левый край -300

    CastHit hit;
    check(cast_shape(mover, at, down_left, floor, {fix32{}, fx(100)}, hit),
          "a body moving down-left into the edge of the floor hits it");
    check(hit.normal == Vec2{fix32{}, fx(-1)},
          "with the normal out of the FLOOR TOP, not the diagonal to its corner");

    // Тот же ход на юнит правее: угол пола больше не лежит на пути угла корпуса, ничьей нет, ответ
    // обязан совпасть. Без этой пары утверждение выше проверяло бы, что свип вообще умеет (0, -1).
    const Vec2 off = {at.x + fx(1), at.y};
    place_box(off, fx(8), fx(16), mover);
    check(cast_shape(mover, off, down_left, floor, {fix32{}, fx(100)}, hit),
          "the same move a unit away from that corner hits the floor too");
    check(hit.normal == Vec2{fix32{}, fx(-1)}, "with the same outward normal");
}

// Вторая ветка остановки — выдохшийся потолок продвижения на ПОЧТИ КАСАТЕЛЬНОМ пути, где узкая фаза
// манифольда не собирает и ответ остаётся за свидетелем. Сцена та же, что у гейта запросов: луч по
// прямой, содержащей верхнюю грань. Утверждение здесь про то, что фолбэк жив, а не про его нормаль.
void test_grazing_path_still_hits() {
    WorldShape ray, target;
    ray.count = 1;
    ray.points[0] = {fix32{}, fx(8)};
    unit_box({fx(50), fix32{}}, target);  // x от 42 до 58, y от -8 до 8

    CastHit hit;
    check(cast_shape(ray, {fix32{}, fx(8)}, {fx(100), fix32{}}, target, {fx(50), fix32{}}, hit),
          "a ray along the line of the top face still reports the corner");
    check(close(hit.fraction, fix32::from_float(0.42), 0.01), "at 42/100, as before the change");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics corner gate\n");
    test_coplanar_faces();
    test_coplanar_faces_vertical();
    test_witness_alone_is_ambiguous();
    test_floor_edge_diagonal();
    test_grazing_path_still_hits();
    std::printf("framework-physics-corner: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
