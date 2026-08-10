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

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics point-core gate\n");
    test_point_core();
    test_nearest_degenerate();
    std::printf("framework-physics-point: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
