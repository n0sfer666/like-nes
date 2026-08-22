#include <cstdio>

#include "body.hpp"
#include "narrowphase.hpp"
#include "nearest.hpp"
#include "platform_args.hpp"

// Путь «точка об ядро» — та половина узкой фазы, которую разделяющая ось закрыть не может: у круга
// ядро вырождено в точку, нормалей граней у неё нет, и пересечение считается РАССТОЯНИЕМ
// (`nearest.cpp`). Отдельной целью от двух других путей узкой фазы намеренно: имя упавшей цели в
// логе CI обязано называть сломанный путь, а не «узкую фазу вообще».
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

void test_point_core() {
    BodyDesc a;
    a.key = 1;
    a.shape = circle(fix32::from_int(10));
    a.position = {fix32{}, fix32{}};
    BodyDesc b = a;
    b.key = 2;
    b.position = {fix32::from_int(15), fix32{}};

    Manifold m;
    check(collide(make_body(a), make_body(b), SPECULATIVE_MARGIN, m), "circle-circle overlap detected");
    check(m.count == 1, "circle-circle gives a single point");
    check(m.points[0].penetration == fix32::from_int(5), "circle-circle penetration = 5");
    check(m.normal == Vec2{fix32::from_int(1), fix32{}}, "circle-circle normal points a->b");

    b.position = {fix32::from_int(21), fix32{}};
    check(!collide(make_body(a), make_body(b), SPECULATIVE_MARGIN, m), "circle-circle apart -> no contact");

    // Совпавшие центры — вырожденный вход, у которого обязан быть ОПРЕДЕЛЁННЫЙ ответ: без него
    // два тела в одной точке разъезжаются как придётся, и golden гуляет от запуска к запуску.
    b.position = a.position;
    check(collide(make_body(a), make_body(b), SPECULATIVE_MARGIN, m), "coincident centres still collide");
    check(m.normal == Vec2{fix32{}, fix32::from_int(1)}, "coincident centres -> fixed normal");

    BodyDesc plate;
    plate.key = 3;
    plate.type = BodyType::Static;
    plate.shape = box(fix32::from_int(50), fix32::from_int(10));
    plate.position = {fix32{}, fix32::from_int(15)};
    check(collide(make_body(a), make_body(plate), SPECULATIVE_MARGIN, m), "circle-box overlap detected");
    check(m.normal == Vec2{fix32{}, fix32::from_int(1)}, "circle-box normal points down to plate");

    // Коробка против круга обязана дать ЗЕРКАЛЬНУЮ нормаль той же величины: это не косметика,
    // а проверка того, что перестановка тел проходит через ту же геометрию, а не через вторую
    // реализацию, которая однажды разойдётся с первой на округлении.
    Manifold flipped;
    check(collide(make_body(plate), make_body(a), SPECULATIVE_MARGIN, flipped), "box-circle overlap detected");
    check(flipped.normal == -m.normal, "box-circle normal is mirrored");
    check(flipped.count == m.count && flipped.points[0].penetration == m.points[0].penetration,
          "box-circle penetration matches");
}

// Различитель вырожденного ответа. `nearest_on_core` обещает вызывающему, что тот сможет отличить
// «направления не было» от обычного ответа, — и путь зазора на это обещание опирается, разворачивая
// найденное направление знаком.
void test_nearest_degenerate() {
    // Точка совпала с ядром: направления нет вовсе, и умолчание обязано быть ОДНОЙ И ТОЙ ЖЕ мировой
    // осью независимо от того, с какой стороны пришёл запрос. Пройди оно через минус во втором
    // цикле `gap.cpp`, и пара разъезжалась бы в разные стороны от нумерации вершин.
    BodyDesc dot_body;
    dot_body.shape = circle(fix32::from_int(3));
    const Body db = make_body(dot_body);
    WorldShape core;
    to_world(db.shape, db.position, db.rot, core);
    const Nearest same = nearest_on_core(core.points[0], core);
    check(same.dir == FALLBACK_DIR, "a point coincident with the core falls back to the world axis");
    check(same.degenerate, "and it reports that through the flag");

    // Вторая половина того же различителя, и она несущая. Вершина, легшая РОВНО на грань, тоже даёт
    // нулевое расстояние — на целочисленных координатах это обычное дело, — но направление у неё
    // вполне определённое: нормаль той грани. Спрашивай про ноль вместо флага, и вызывающий менял
    // бы правильную нормаль на мировую ось, то есть толкал пару не туда, куда она разъезжается.
    BodyDesc square_body;
    square_body.shape = box(fix32::from_int(10), fix32::from_int(10));
    const Body sb = make_body(square_body);
    WorldShape square;
    to_world(sb.shape, sb.position, sb.rot, square);
    const Nearest on_face = nearest_on_core({fix32{}, fix32::from_int(10)}, square);
    check(on_face.dist.raw == 0, "control: a point exactly on a face is at zero distance too");
    check(!on_face.degenerate, "a point on a face is not degenerate");
    check(on_face.dir == Vec2{fix32{}, fix32::from_int(1)}, "and it keeps the face normal");
}

// Нормаль у ДЛИННОЙ грани. Отдельным случаем от вырожденного различителя, потому что вопрос другой:
// там — «отличим ли ответ без направления», здесь — «верно ли направление, когда оно есть».
//
// Дефект, который случай пинит: ближайшая точка на грани считалась умножением ребра на параметр в
// Q16.16, то есть с квантом `длина_ребра / 65536` ВДОЛЬ грани. На ребре в 400 юнитов это 0.006, и у
// точки, стоящей в 0.006 ПОПЕРЁК грани, продольная и поперечная составляющие разности сравнимы —
// нормировка давала (-0.91, -0.41) вместо (-1, 0). В движке это выглядело так: персонаж, въехавший
// в стену на 2048 юнит/с, скользил вдоль ложной нормали и улетал ВВЕРХ со скоростью 942 юнит/с.
//
// Порог допуска взят на два порядка меньше найденного отклонения и на два больше кванта Q16.16:
// допуск, в который пролезает старая реализация, ничего бы не пинил.
void test_long_face_normal() {
    BodyDesc wall;
    // Стена вчетверо длиннее, чем насыщается квадрат длины ребра в Q16.16 (182 юнита), и тоньше
    // кванта позиции по другой оси — ровно та геометрия, на которой считается тайловая полоса.
    wall.shape = box(fix32::from_float(0.25), fix32::from_int(200));
    const Body wb = make_body(wall);
    WorldShape core;
    to_world(wb.shape, wb.position, wb.rot, core);

    constexpr fix32 TOL = fix32::from_float(0.001);
    // Расстояние сверяется ПОЛОСОЙ шириной в несколько raw, а не «меньше ожидаемого»: односторонний
    // порог пропускает и реализацию, отвечающую нулём, и старую — та на высоте 190 давала 0.00648,
    // то есть промахивалась на 0.00048 и в допуск 0.001 пролезала. Полоса же в четыре raw (6e-5)
    // отбивает её на всех трёх высотах. Ответ обязан быть ТОЧНЫМ: нормаль осевой грани единична бит
    // в бит, а `farthest` при ней сводится к вычитанию координат.
    const fix32 HX = fix32::from_float(0.25);
    const fix32 PX = fix32::from_float(-0.256);
    const fix32 want = -HX - PX;
    constexpr fix32 DIST_TOL = fix32::from_raw(4);
    // Высоты взяты НЕДВОИЧНЫЕ по параметру вдоль ребра — 237/400, 311/400, 390/400. Прежняя тройка
    // (0, 50, 190) была тут ошибкой: у первых двух параметр равен 0.5 и 0.625, то есть представим в
    // Q16.16 ТОЧНО, квант вдоль ребра не возникает вовсе, и старая реализация проходила их обе.
    // Различала дефект одна высота из трёх, хотя комментарий обещал три.
    const fix32 heights[3] = {fix32::from_int(37), fix32::from_int(111), fix32::from_int(190)};
    for (int i = 0; i < 3; ++i) {
        const Vec2 p = {PX, heights[i]};
        const Nearest n = nearest_on_core(p, core);
        const fix32 minus_one = fix32::from_float(-1);
        check(minus_one - TOL < n.dir.x && n.dir.x < minus_one + TOL,
              "the long face keeps a unit outward normal");
        check(n.dir.y < TOL && -TOL < n.dir.y, "and no tangential component from the edge quantum");
        check(want - DIST_TOL < n.dist && n.dist < want + DIST_TOL,
              "and the plane distance is exact");
    }

    // Вершинная область на общем пути: там ближайшая точка — сама вершина, значение точное, и
    // нормаль обязана быть диагональной, а не нормалью грани. Без этого случая ветка «внутри грани»
    // могла бы отвечать на ВСЕ запросы и гейт молчал бы.
    const Vec2 corner = {fix32::from_float(-0.25) - fix32::from_int(3), fix32::from_int(203)};
    const Nearest cn = nearest_on_core(corner, core);
    check(cn.dir.x < fix32{} && cn.dir.y.raw > 0, "past the end of the face the normal turns to the corner");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics point-core gate\n");
    test_point_core();
    test_nearest_degenerate();
    test_long_face_normal();
    std::printf("framework-physics-point: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
